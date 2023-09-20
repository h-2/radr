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
#include <ranges>

#include "borrow.hpp"
#include "concepts.hpp"
#include "detail/copyable_box.hpp"
#include "detail/detail.hpp"
#include "generator.hpp"
#include "rad_interface.hpp"

namespace radr::detail
{

template <class URange>
struct filter_iterator_category
{};

template <std::ranges::forward_range URange>
struct filter_iterator_category<URange>
{
    using Cat = typename std::iterator_traits<std::ranges::iterator_t<URange>>::iterator_category;
    // clang-format off
    using iterator_category =
        std::conditional_t<std::derived_from<Cat, std::bidirectional_iterator_tag>, std::bidirectional_iterator_tag,
        std::conditional_t<std::derived_from<Cat, std::forward_iterator_tag>,       std::forward_iterator_tag,
                           /*else*/                                                 Cat>>;
    // clang-format on
};

template <typename Pred, typename URange>
concept filter_pred_constraints = std::is_object_v<Pred> && std::copy_constructible<Pred> &&
                                  std::indirect_unary_predicate<Pred const, std::ranges::iterator_t<URange>>;

} // namespace radr::detail

namespace radr
{

template <unqualified_forward_range                                           URange,
          bool                                                                Const,
          detail::filter_pred_constraints<detail::maybe_const<Const, URange>> Pred>
class filter_iterator : public detail::filter_iterator_category<URange>
{
    using URangeC = detail::maybe_const<Const, URange>;


    template <unqualified_forward_range                                             URange_,
              bool                                                                  Const_,
              detail::filter_pred_constraints<detail::maybe_const<Const_, URange_>> Pred_>
    friend class filter_iterator;

    [[no_unique_address]] std::ranges::iterator_t<URangeC> current_{};
    [[no_unique_address]] std::ranges::sentinel_t<URangeC> end_{};
    [[no_unique_address]] detail::copyable_box<Pred> pred_;

public:
    // clang-format off
    using iterator_concept =
        std::conditional_t<std::ranges::bidirectional_range<URangeC>, std::bidirectional_iterator_tag,
        std::conditional_t<std::ranges::forward_range<URangeC>,       std::forward_iterator_tag,
                           /* else */                                 std::input_iterator_tag>>;
    // clang-format on

    // using iterator_category = inherited;
    using value_type      = std::ranges::range_value_t<URangeC>;
    using difference_type = std::ranges::range_difference_t<URangeC>;

    filter_iterator() = default;

    constexpr filter_iterator(Pred                             pred,
                              std::ranges::iterator_t<URangeC> current,
                              std::ranges::sentinel_t<URangeC> end) :
      pred_(std::in_place, std::move(pred)), current_(std::move(current)), end_(std::move(end))
    {}

    // Note: `i` should always be `iterator<false>`, but directly using
    // `iterator<false>` is ill-formed when `Const` is false
    // (see http://wg21.link/class.copy.ctor#5).
    template <bool Const_>
    constexpr filter_iterator(filter_iterator<URange, Const_, Pred> i)
        requires(Const && (!Const_) &&
                 std::convertible_to<std::ranges::iterator_t<URange>, std::ranges::iterator_t<URangeC>>)
      : pred_(std::move(i.pred_)), current_(std::move(i.current_)), end_(std::move(i.end_))
    {}

    constexpr std::ranges::iterator_t<URangeC> const & base() const & noexcept { return current_; }
    constexpr std::ranges::iterator_t<URangeC>         base() && { return std::move(current_); }

    constexpr std::ranges::range_reference_t<URangeC> operator*() const { return *current_; }
    constexpr std::ranges::iterator_t<URangeC>        operator->() const
        requires detail::has_arrow<std::ranges::iterator_t<URangeC>>
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
        requires std::ranges::bidirectional_range<URangeC>
    {
        do
        {
            --current_;
        }
        while (!std::invoke(*pred_, *current_));
        return *this;
    }
    constexpr filter_iterator operator--(int)
        requires std::ranges::bidirectional_range<URangeC>
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
        requires (!std::ranges::common_range<URangeC>)
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
        requires std::indirectly_swappable<std::ranges::iterator_t<URangeC>>
    {
        std::ranges::iter_swap(x.current_, y.current_);
        std::ranges::iter_swap(x.end_, y.end_);
    }
};

inline constexpr auto filter_borrow =
  []<const_borrowable_range URange, detail::filter_pred_constraints<URange> Fn>(URange && urange, Fn fn)
{
    using iterator_t = filter_iterator<std::remove_cvref_t<URange>, const_symmetric_range<URange>, Fn>;
    using sentinel_t = std::conditional_t<std::ranges::common_range<URange>, iterator_t, std::default_sentinel_t>;

    using const_iterator_t = filter_iterator<std::remove_cvref_t<URange>, true, Fn>;
    using const_sentinel_t =
      std::conditional_t<std::ranges::common_range<URange const>, const_iterator_t, std::default_sentinel_t>;

    using BorrowingRad =
      borrowing_rad<iterator_t, sentinel_t, const_iterator_t, const_sentinel_t, borrowing_rad_kind::unsized>;

    // eagerly search for begin
    auto begin = std::ranges::find_if(std::ranges::begin(urange), std::ranges::end(urange), std::ref(*fn));

    if constexpr (std::ranges::common_range<URange>)
    {
        return BorrowingRad{
          iterator_t{fn,         std::move(begin), std::ranges::end(urange)},
          iterator_t{fn, std::ranges::end(urange), std::ranges::end(urange)}
        };
    }
    else
    {
        return BorrowingRad{
          iterator_t{fn, std::move(begin), std::ranges::end(urange)},
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

} // namespace radr

namespace radr::pipe
{
inline namespace cpo
{
inline constexpr auto filter = detail::pipe_with_args_fn{filter_coro, filter_borrow};
} // namespace cpo
} // namespace radr::pipe
