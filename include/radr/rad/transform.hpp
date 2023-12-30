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

#include <functional>
#include <iterator>
#include <ranges>

#include "../concepts.hpp"
#include "../detail/copyable_box.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../generator.hpp"

namespace radr::detail::transform
{

template <class Iter, class Fn>
concept fn_constraints = std::is_object_v<Fn> && std::copy_constructible<Fn> &&
                         std::regular_invocable<Fn const &, std::iter_reference_t<Iter>> &&
                         can_reference<std::invoke_result_t<Fn const &, std::iter_reference_t<Iter>>>;

template <class Iter>
struct iterator_concept
{
    using type = std::input_iterator_tag;
};

template <std::random_access_iterator Iter>
struct iterator_concept<Iter>
{
    using type = std::random_access_iterator_tag;
};

template <std::bidirectional_iterator Iter>
struct iterator_concept<Iter>
{
    using type = std::bidirectional_iterator_tag;
};

template <std::forward_iterator Iter>
struct iterator_concept<Iter>
{
    using type = std::forward_iterator_tag;
};

template <class, class>
struct iterator_category_base
{};

template <std::forward_iterator Iter, class Fn>
struct iterator_category_base<Iter, Fn>
{
    using Cat = typename std::iterator_traits<Iter>::iterator_category;

    using iterator_category = std::conditional_t<
      std::is_reference_v<std::invoke_result_t<Fn const &, std::iter_reference_t<Iter>>>,
      std::conditional_t<std::derived_from<Cat, std::contiguous_iterator_tag>, std::random_access_iterator_tag, Cat>,
      std::input_iterator_tag>;
};

} // namespace radr::detail::transform

namespace radr::detail
{

template <std::forward_iterator Iter, std::sentinel_for<Iter> Sent, typename Fn>
    requires detail::transform::fn_constraints<Iter, Fn>
class transform_sentinel;

template <std::forward_iterator Iter, typename Fn>
    requires detail::transform::fn_constraints<Iter, Fn>
class transform_iterator : public detail::transform::iterator_category_base<Iter, Fn>
{
    [[no_unique_address]] copyable_box<Fn> func_;

    template <std::forward_iterator Iter_, typename Fn_>
        requires detail::transform::fn_constraints<Iter_, Fn_>
    friend class transform_iterator;
    template <std::forward_iterator Iter_, std::sentinel_for<Iter_> Sent_, typename Fn_>
        requires detail::transform::fn_constraints<Iter_, Fn_>
    friend class transform_sentinel;

public:
    Iter current_ = Iter();

    using iterator_concept = typename detail::transform::iterator_concept<Iter>::type;
    using value_type       = std::remove_cvref_t<std::invoke_result_t<Fn const &, std::iter_reference_t<Iter>>>;
    using difference_type  = std::iter_difference_t<Iter>;

    transform_iterator()
        requires(std::default_initializable<copyable_box<Fn>> && std::default_initializable<Iter>)
    = default;

    constexpr transform_iterator(Fn fn, Iter current) :
      func_(std::in_place, std::move(fn)), current_(std::move(current))
    {}

    template <detail::different_from<Iter> OtherIter>
    constexpr transform_iterator(transform_iterator<OtherIter, Fn> i)
        requires std::convertible_to<OtherIter, Iter>
      : func_(std::move(i.func_)), current_(std::move(i.current_))
    {}

    constexpr Iter const & base() const & noexcept { return current_; }

    constexpr Iter base() && { return std::move(current_); }

    constexpr decltype(auto) operator*() const noexcept(noexcept(std::invoke(*func_, *current_)))
    {
        return std::invoke(*func_, *current_);
    }

    constexpr transform_iterator & operator++()
    {
        ++current_;
        return *this;
    }

    constexpr void operator++(int) { ++current_; }

    constexpr transform_iterator operator++(int)
        requires std::forward_iterator<Iter>
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr transform_iterator & operator--()
        requires std::bidirectional_iterator<Iter>
    {
        --current_;
        return *this;
    }

    constexpr transform_iterator operator--(int)
        requires std::bidirectional_iterator<Iter>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr transform_iterator & operator+=(difference_type n)
        requires std::random_access_iterator<Iter>
    {
        current_ += n;
        return *this;
    }

    constexpr transform_iterator & operator-=(difference_type n)
        requires std::random_access_iterator<Iter>
    {
        current_ -= n;
        return *this;
    }

    constexpr decltype(auto) operator[](difference_type n) const noexcept(noexcept(std::invoke(*func_, current_[n])))
        requires std::random_access_iterator<Iter>
    {
        return std::invoke(*func_, current_[n]);
    }

    friend constexpr bool operator==(transform_iterator const & x, transform_iterator const & y)
        requires std::equality_comparable<Iter>
    {
        return x.current_ == y.current_;
    }

    friend constexpr bool operator<(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<Iter>
    {
        return x.current_ < y.current_;
    }

    friend constexpr bool operator>(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<Iter>
    {
        return x.current_ > y.current_;
    }

    friend constexpr bool operator<=(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<Iter>
    {
        return x.current_ <= y.current_;
    }

    friend constexpr bool operator>=(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<Iter>
    {
        return x.current_ >= y.current_;
    }

    friend constexpr auto operator<=>(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<Iter> && std::three_way_comparable<Iter>
    {
        return x.current_ <=> y.current_;
    }

    friend constexpr transform_iterator operator+(transform_iterator i, difference_type n)
        requires std::random_access_iterator<Iter>
    {
        return transform_iterator{*i.func_, i.current_ + n};
    }

    friend constexpr transform_iterator operator+(difference_type n, transform_iterator i)
        requires std::random_access_iterator<Iter>
    {
        return transform_iterator{*i.func_, i.current_ + n};
    }

    friend constexpr transform_iterator operator-(transform_iterator i, difference_type n)
        requires std::random_access_iterator<Iter>
    {
        return transform_iterator{*i.func_, i.current_ - n};
    }

    friend constexpr difference_type operator-(transform_iterator const & x, transform_iterator const & y)
        requires std::sized_sentinel_for<Iter, Iter>
    {
        return x.current_ - y.current_;
    }

    friend constexpr decltype(auto) iter_move(transform_iterator const & i) noexcept(noexcept(*i))
    {
        if constexpr (std::is_lvalue_reference_v<decltype(*i)>)
            return std::move(*i);
        else
            return *i;
    }
};

template <std::forward_iterator Iter, std::sentinel_for<Iter> Sen, typename Fn>
    requires detail::transform::fn_constraints<Iter, Fn>
class transform_sentinel
{
    Sen end_{};

    // template <std::forward_iterator Iter_, typename Fn_>
    //     requires detail::transform::fn_constraints<Iter_, Fn_>
    // friend class transform_iterator;
    template <std::forward_iterator Iter_, std::sentinel_for<Iter_> Sent_, typename Fn_>
        requires detail::transform::fn_constraints<Iter_, Fn_>
    friend class transform_sentinel;

public:
    transform_sentinel() = default;

    constexpr explicit transform_sentinel(Sen end) : end_(end) {}

    constexpr transform_sentinel(Fn, Sen end) : end_(end) {}

    template <std::forward_iterator OtherIter, detail::different_from<Sen> OtherSent>
    constexpr transform_sentinel(transform_sentinel<OtherIter, OtherSent, Fn> i)
        requires std::convertible_to<OtherSent, Sen>
      : end_(std::move(i.end_))
    {}

    constexpr Sen base() const { return end_; }

    friend constexpr bool operator==(transform_iterator<Iter, Fn> const & x, transform_sentinel const & y)
    {
        return x.current_ == y.end_;
    }

    friend constexpr std::ranges::range_difference_t<transform_iterator<Iter, Fn>> operator-(
      transform_iterator<Iter, Fn> const & x,
      transform_sentinel const &           y)
    {
        return x.current_ - y.end_;
    }

    friend constexpr std::ranges::range_difference_t<transform_iterator<Iter, Fn>> operator-(
      transform_sentinel const &           x,
      transform_iterator<Iter, Fn> const & y)
    {
        return x.end_ - y.current_;
    }
};

inline constexpr auto transform_borrow =
  []<const_borrowable_range URange, std::copy_constructible Fn>(URange && urange, Fn fn)
    requires(detail::transform::fn_constraints<radr::iterator_t<URange>, Fn>)
{
    using iterator_t = transform_iterator<radr::iterator_t<URange>, Fn>;
    using sentinel_t = std::conditional_t<std::ranges::common_range<URange>,
                                          iterator_t,
                                          transform_sentinel<radr::iterator_t<URange>, radr::sentinel_t<URange>, Fn>>;

    using const_iterator_t = transform_iterator<radr::const_iterator_t<URange>, Fn>;
    using const_sentinel_t =
      std::conditional_t<std::ranges::common_range<URange const>,
                         const_iterator_t,
                         transform_sentinel<radr::const_iterator_t<URange>, radr::const_sentinel_t<URange>, Fn>>;

    if constexpr (std::ranges::sized_range<URange>)
    {
        using BorrowingRad =
          borrowing_rad<iterator_t, sentinel_t, const_iterator_t, const_sentinel_t, borrowing_rad_kind::sized>;

        return BorrowingRad{
          iterator_t{fn, radr::begin(urange)},
          sentinel_t{fn,   radr::end(urange)},
          std::ranges::size(urange)
        };
    }
    else
    {
        using BorrowingRad =
          borrowing_rad<iterator_t, sentinel_t, const_iterator_t, const_sentinel_t, borrowing_rad_kind::unsized>;

        return BorrowingRad{
          iterator_t{fn, radr::begin(urange)},
          sentinel_t{fn,   radr::end(urange)}
        };
    }
};

inline constexpr auto transform_coro = []<movable_range URange, std::copy_constructible Fn>(URange && urange, Fn fn)
    requires(std::regular_invocable<Fn &, std::ranges::range_reference_t<URange>> &&
             can_reference<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);

    // we need to create inner functor so that it can take by value
    return
      [](auto urange_, Fn fn) -> radr::generator<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>
    {
        for (auto && elem : urange_)
            co_yield fn(std::forward<decltype(elem)>(elem));
    }(std::move(urange), std::move(fn));
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto transform = detail::pipe_with_args_fn{detail::transform_coro, detail::transform_borrow};
} // namespace cpo
} // namespace radr
