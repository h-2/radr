// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 The LLVM Project
// Copyright (c) 2023 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>
#include <functional>
#include <iterator>
#include <ranges>

#include "../concepts.hpp"
#include "../detail/copyable_box.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../generator.hpp"
#include "../range_access.hpp"

namespace radr::detail
{

template <class Iter>
struct filter_iterator_category
{};

template <std::forward_iterator Iter>
struct filter_iterator_category<Iter>
{
    using Cat = typename std::iterator_traits<Iter>::iterator_category;
    // clang-format off
    using iterator_category =
        std::conditional_t<std::derived_from<Cat, std::bidirectional_iterator_tag>, std::bidirectional_iterator_tag,
        std::conditional_t<std::derived_from<Cat, std::forward_iterator_tag>,       std::forward_iterator_tag,
                           /*else*/                                                 Cat>>;
    // clang-format on
};

template <typename Pred, typename Iter>
concept filter_pred_constraints =
  std::is_object_v<Pred> && std::copy_constructible<Pred> && std::indirect_unary_predicate<Pred const, Iter>;

template <std::forward_iterator Iter, std::sentinel_for<Iter> Sent, filter_pred_constraints<Iter> Pred>
class filter_iterator : public filter_iterator_category<Iter>
{
    template <std::forward_iterator Iter2, std::sentinel_for<Iter2> Sent2, filter_pred_constraints<Iter2> Pred2>
    friend class filter_iterator;

    [[no_unique_address]] copyable_box<Pred> pred_;
    [[no_unique_address]] Iter               current_{};
    [[no_unique_address]] Sent               end_{};

public:
    // clang-format off
    using iterator_concept =
        std::conditional_t<std::bidirectional_iterator<Iter>, std::bidirectional_iterator_tag,
        std::conditional_t<std::forward_iterator<Iter>,       std::forward_iterator_tag,
                           /* else */                         std::input_iterator_tag>>;
    // clang-format on

    // using iterator_category = inherited;
    using value_type      = std::iter_value_t<Iter>;
    using difference_type = std::iter_difference_t<Iter>;

    filter_iterator() = default;

    constexpr filter_iterator(Pred pred, Iter current, Sent end) :
      pred_(std::in_place, std::move(pred)), current_(std::move(current)), end_(std::move(end))
    {}

    // Note: `i` should always be `iterator<false>`, but directly using
    // `iterator<false>` is ill-formed when `Const` is false
    // (see http://wg21.link/class.copy.ctor#5).
    template <detail::different_from<Iter> OtherIter, typename OtherSent>
    constexpr filter_iterator(filter_iterator<OtherIter, OtherSent, Pred> i)
        requires(std::convertible_to<OtherIter, Iter>)
      : pred_(std::move(i.pred_)), current_(std::move(i.current_)), end_(std::move(i.end_))
    {}

    constexpr Iter const & base() const & noexcept { return current_; }
    constexpr Iter         base() && { return std::move(current_); }

    constexpr std::iter_reference_t<Iter> operator*() const { return *current_; }
    constexpr Iter                        operator->() const
        requires has_arrow<Iter>
    {
        return current_;
    }

    constexpr filter_iterator & operator++()
    {
        current_ = std::ranges::find_if(std::move(++current_), end_, std::ref(*pred_));
        return *this;
    }

    constexpr filter_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr filter_iterator & operator--()
        requires std::bidirectional_iterator<Iter>
    {
        do
        {
            --current_;
        }
        while (!std::invoke(*pred_, *current_));
        return *this;
    }
    constexpr filter_iterator operator--(int)
        requires std::bidirectional_iterator<Iter>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    friend constexpr bool operator==(filter_iterator const & x, filter_iterator const & y)
    {
        return x.current_ == y.current_;
    }

    friend constexpr bool operator==(filter_iterator const & x, std::default_sentinel_t const &)
        requires(!std::same_as<Iter, Sent>)
    {
        return x.current_ == x.end_;
    }

    //TODO what do we do with this
    // friend constexpr std::ranges::range_rvalue_reference_t<URangeC> iter_move(filter_iterator const & it) noexcept(
    //   noexcept(std::ranges::iter_move(it.current_)))
    // {
    //     return std::ranges::iter_move(it.current_);
    // }

    friend constexpr void iter_swap(filter_iterator const & x, filter_iterator const & y) noexcept(
      noexcept(std::ranges::iter_swap(x.current_, y.current_)))
        requires std::indirectly_swappable<Iter>
    {
        std::ranges::iter_swap(x.current_, y.current_);
        std::ranges::iter_swap(x.end_, y.end_);
    }
};

inline constexpr auto filter_borrow =
  []<const_borrowable_range URange, filter_pred_constraints<std::ranges::iterator_t<URange>> Fn>(URange && urange,
                                                                                                 Fn        fn)
{
    using iterator_t = filter_iterator<radr::iterator_t<URange>, radr::sentinel_t<URange>, Fn>;
    using sentinel_t = std::conditional_t<std::ranges::common_range<URange>, iterator_t, std::default_sentinel_t>;

    using URangeC          = std::remove_cvref_t<URange> const;
    using const_iterator_t = filter_iterator<radr::iterator_t<URangeC>, radr::sentinel_t<URangeC>, Fn>;
    using const_sentinel_t =
      std::conditional_t<std::ranges::common_range<URange const>, const_iterator_t, std::default_sentinel_t>;

    using BorrowingRad =
      borrowing_rad<iterator_t, sentinel_t, const_iterator_t, const_sentinel_t, borrowing_rad_kind::unsized>;

    // eagerly search for begin
    auto begin = std::ranges::find_if(radr::begin(urange), radr::end(urange), std::ref(fn));

    if constexpr (std::ranges::common_range<URange>)
    {
        return BorrowingRad{
          iterator_t{fn,  std::move(begin), radr::end(urange)},
          iterator_t{fn, radr::end(urange), radr::end(urange)}
        };
    }
    else
    {
        return BorrowingRad{
          iterator_t{fn, std::move(begin), radr::end(urange)},
          std::default_sentinel
        };
    }
};

inline constexpr auto filter_coro =
  []<movable_range URange, std::indirect_unary_predicate<std::ranges::iterator_t<URange>> Fn>(URange && urange, Fn fn)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);

    // we need to create inner functor so that it can take by value
    return [](auto urange_,
              Fn   fn_) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        for (auto && elem : urange_)
            if (fn_(elem))
                co_yield elem;
    }(std::move(urange), std::move(fn));
};

} // namespace radr::detail

namespace radr
{
inline namespace cpo
{
inline constexpr auto filter = detail::pipe_with_args_fn{detail::filter_coro, detail::filter_borrow};
} // namespace cpo
} // namespace radr
