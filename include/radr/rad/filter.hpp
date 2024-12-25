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
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../detail/semiregular_box.hpp"
#include "../generator.hpp"
#include "../range_access.hpp"

namespace radr::detail
{

template <typename Func1, typename Func2>
struct and_fn
{
    [[no_unique_address]] Func1 func1;
    [[no_unique_address]] Func2 func2;

    constexpr bool operator()(auto && val) const noexcept(noexcept(func1(val)) && noexcept(func2(val)))
    {
        return func1(val) && func2(val);
    }
};

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

template <typename Func, typename Iter>
concept filter_func_constraints =
  std::is_object_v<Func> && std::copy_constructible<Func> && std::indirect_unary_predicate<Func const, Iter>;

template <std::forward_iterator Iter, std::sentinel_for<Iter> Sent, filter_func_constraints<Iter> Func>
class filter_iterator : public filter_iterator_category<Iter>
{
    template <std::forward_iterator Iter2, std::sentinel_for<Iter2> Sent2, filter_func_constraints<Iter2> Func2>
    friend class filter_iterator;

    [[no_unique_address]] semiregular_box<Func> func_;
    [[no_unique_address]] Iter                  current_{};
    [[no_unique_address]] Sent                  end_{};

    template <typename Container>
    constexpr friend auto tag_invoke(custom::rebind_iterator_tag,
                                     filter_iterator it,
                                     Container &     container_old,
                                     Container &     container_new)
    {
        it.current_ = tag_invoke(custom::rebind_iterator_tag{}, it.current_, container_old, container_new);
        it.end_     = tag_invoke(custom::rebind_iterator_tag{}, it.end_, container_old, container_new);
        return it;
    }

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

    constexpr filter_iterator(Func func, Iter current, Sent end) :
      func_(std::in_place, std::move(func)), current_(std::move(current)), end_(std::move(end))
    {}

    //!\brief Convert from non-const.
    template <detail::different_from<Iter> OtherIter, typename OtherSent>
    constexpr filter_iterator(filter_iterator<OtherIter, OtherSent, Func> i)
        requires(std::convertible_to<OtherIter, Iter> && std::convertible_to<OtherSent, Sent>)
      : func_(std::move(i.func_)), current_(std::move(i.current_)), end_(std::move(i.end_))
    {}

    constexpr Iter const & base_iter() const & noexcept { return current_; }
    constexpr Iter         base_iter() && { return std::move(current_); }

    constexpr Sent const & base_sent() const & noexcept { return end_; }
    constexpr Sent         base_sent() && { return std::move(end_); }

    constexpr Func const & func() const & noexcept { return *func_; }
    constexpr Func         func() && { return std::move(*func_); }

    constexpr std::iter_reference_t<Iter> operator*() const { return *current_; }
    constexpr Iter                        operator->() const
        requires has_arrow<Iter>
    {
        return current_;
    }

    constexpr filter_iterator & operator++()
    {
        current_ = std::ranges::find_if(std::move(++current_), end_, std::ref(*func_));
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
        while (!std::invoke(*func_, *current_));
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

// iterator-based borrow
inline constexpr auto filter_borrow_impl =
  []<typename UIt, typename USen, typename UCIt, typename UCSen, typename Fn_>(UIt  it,
                                                                               USen sen,
                                                                               UCIt,
                                                                               UCSen,
                                                                               Fn_ _fn) // generic case
{
    using iterator_t       = filter_iterator<UIt, USen, Fn_>;
    using sentinel_t       = std::default_sentinel_t;
    using const_iterator_t = filter_iterator<UCIt, UCSen, Fn_>;
    using const_sentinel_t = std::default_sentinel_t;

    using BorrowingRad =
      borrowing_rad<iterator_t, sentinel_t, const_iterator_t, const_sentinel_t, borrowing_rad_kind::unsized>;

    // eagerly search for begin
    auto begin = std::ranges::find_if(it, sen, std::ref(_fn));

    return BorrowingRad{
      iterator_t{_fn, std::move(begin), std::move(sen)},
      std::default_sentinel
    };
};

inline constexpr auto filter_borrow =
  []<const_borrowable_range URange, filter_func_constraints<radr::iterator_t<URange>> Fn>(URange && urange, Fn fn)
{
    // dispatch between generic case and chained case(s)
    return overloaded{filter_borrow_impl,
                      []<typename UIt, typename USen, typename UCIt, typename UCSen, typename UFn, typename Fn_>(
                        filter_iterator<UIt, USen, UFn> iter,
                        std::default_sentinel_t,
                        filter_iterator<UCIt, UCSen, UFn> citer,
                        std::default_sentinel_t,
                        Fn_ new_fn)
    {
        return filter_borrow_impl(std::move(iter).base_iter(),
                                  std::move(iter).base_sent(),
                                  std::move(citer).base_iter(),
                                  std::move(citer).base_sent(),
                                  and_fn{std::move(iter).func(), std::move(new_fn)});
    }}(radr::begin(urange), radr::end(urange), radr::cbegin(urange), radr::cend(urange), std::move(fn));
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
