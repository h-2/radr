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
#include <ranges>

#include "concepts.hpp"
#include "detail/copyable_box.hpp"
#include "detail/detail.hpp"
#include "detail/pipe.hpp"
#include "generator.hpp"

namespace radr::detail
{

template <class Fn, class URange>
concept regular_invocable_with_range_ref = std::regular_invocable<Fn, std::ranges::range_reference_t<URange>>;

}

namespace radr::detail::transform
{

template <class URange, class Fn>
concept constraints = std::ranges::forward_range<URange> && std::is_object_v<Fn> && std::copy_constructible<Fn> &&
                      std::regular_invocable<Fn const &, std::ranges::range_reference_t<URange>> &&
                      detail::can_reference<std::invoke_result_t<Fn const &, std::ranges::range_reference_t<URange>>>;

template <class URange>
struct iterator_concept
{
    using type = std::input_iterator_tag;
};

template <std::ranges::random_access_range URange>
struct iterator_concept<URange>
{
    using type = std::random_access_iterator_tag;
};

template <std::ranges::bidirectional_range URange>
struct iterator_concept<URange>
{
    using type = std::bidirectional_iterator_tag;
};

template <std::ranges::forward_range URange>
struct iterator_concept<URange>
{
    using type = std::forward_iterator_tag;
};

template <class, class>
struct iterator_category_base
{};

template <std::ranges::forward_range URange, class Fn>
struct iterator_category_base<URange, Fn>
{
    using Cat = typename std::iterator_traits<std::ranges::iterator_t<URange>>::iterator_category;

    using iterator_category = std::conditional_t<
      std::is_reference_v<std::invoke_result_t<Fn const &, std::ranges::range_reference_t<URange>>>,
      std::conditional_t<std::derived_from<Cat, std::contiguous_iterator_tag>, std::random_access_iterator_tag, Cat>,
      std::input_iterator_tag>;
};

} // namespace radr::detail::transform

namespace radr
{

template <typename URange, typename Fn, bool Const>
    requires detail::transform::constraints<URange, Fn>
class transform_sentinel;

template <typename URange, typename Fn, bool Const>
    requires detail::transform::constraints<URange, Fn>
class transform_iterator : public detail::transform::iterator_category_base<URange, Fn>
{
    using Base = detail::maybe_const<Const, URange>;

    [[no_unique_address]] detail::copyable_box<Fn> func_;

    template <typename URange_, typename Fn_, bool Const_>
        requires detail::transform::constraints<URange, Fn>
    friend class transform_iterator;
    template <typename URange_, typename Fn_, bool Const_>
        requires detail::transform::constraints<URange, Fn>
    friend class transform_sentinel;

public:
    std::ranges::iterator_t<Base> current_ = std::ranges::iterator_t<Base>();

    using iterator_concept = typename detail::transform::iterator_concept<URange>::type;
    using value_type      = std::remove_cvref_t<std::invoke_result_t<Fn const &, std::ranges::range_reference_t<Base>>>;
    using difference_type = std::ranges::range_difference_t<Base>;

    transform_iterator()
        requires(std::default_initializable<detail::copyable_box<Fn>> &&
                 std::default_initializable<std::ranges::iterator_t<Base>>)
    = default;

    constexpr transform_iterator(Fn fn, std::ranges::iterator_t<Base> current) :
      func_(std::in_place, std::move(fn)), current_(std::move(current))
    {}

    // Note: `i` should always be `transform_iterator<false>`, but directly using
    // `transform_iterator<false>` is ill-formed when `Const` is false
    // (see http://wg21.link/class.copy.ctor#5).
    constexpr transform_iterator(transform_iterator<URange, Fn, !Const> i)
        requires Const && std::convertible_to<std::ranges::iterator_t<URange>, std::ranges::iterator_t<Base>>
      : func_(std::move(i.func_)), current_(std::move(i.current_))
    {}

    constexpr std::ranges::iterator_t<Base> const & base() const & noexcept { return current_; }

    constexpr std::ranges::iterator_t<Base> base() && { return std::move(current_); }

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
        requires std::ranges::forward_range<Base>
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr transform_iterator & operator--()
        requires std::ranges::bidirectional_range<Base>
    {
        --current_;
        return *this;
    }

    constexpr transform_iterator operator--(int)
        requires std::ranges::bidirectional_range<Base>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr transform_iterator & operator+=(difference_type n)
        requires std::ranges::random_access_range<Base>
    {
        current_ += n;
        return *this;
    }

    constexpr transform_iterator & operator-=(difference_type n)
        requires std::ranges::random_access_range<Base>
    {
        current_ -= n;
        return *this;
    }

    constexpr decltype(auto) operator[](difference_type n) const noexcept(noexcept(std::invoke(*func_, current_[n])))
        requires std::ranges::random_access_range<Base>
    {
        return std::invoke(*func_, current_[n]);
    }

    friend constexpr bool operator==(transform_iterator const & x, transform_iterator const & y)
        requires std::equality_comparable<std::ranges::iterator_t<Base>>
    {
        return x.current_ == y.current_;
    }

    friend constexpr bool operator<(transform_iterator const & x, transform_iterator const & y)
        requires std::ranges::random_access_range<Base>
    {
        return x.current_ < y.current_;
    }

    friend constexpr bool operator>(transform_iterator const & x, transform_iterator const & y)
        requires std::ranges::random_access_range<Base>
    {
        return x.current_ > y.current_;
    }

    friend constexpr bool operator<=(transform_iterator const & x, transform_iterator const & y)
        requires std::ranges::random_access_range<Base>
    {
        return x.current_ <= y.current_;
    }

    friend constexpr bool operator>=(transform_iterator const & x, transform_iterator const & y)
        requires std::ranges::random_access_range<Base>
    {
        return x.current_ >= y.current_;
    }

    friend constexpr auto operator<=>(transform_iterator const & x, transform_iterator const & y)
        requires std::ranges::random_access_range<Base> && std::three_way_comparable<std::ranges::iterator_t<Base>>
    {
        return x.current_ <=> y.current_;
    }

    friend constexpr transform_iterator operator+(transform_iterator i, difference_type n)
        requires std::ranges::random_access_range<Base>
    {
        return transform_iterator{*i.func_, i.current_ + n};
    }

    friend constexpr transform_iterator operator+(difference_type n, transform_iterator i)
        requires std::ranges::random_access_range<Base>
    {
        return transform_iterator{*i.func_, i.current_ + n};
    }

    friend constexpr transform_iterator operator-(transform_iterator i, difference_type n)
        requires std::ranges::random_access_range<Base>
    {
        return transform_iterator{*i.func_, i.current_ - n};
    }

    friend constexpr difference_type operator-(transform_iterator const & x, transform_iterator const & y)
        requires std::sized_sentinel_for<std::ranges::iterator_t<Base>, std::ranges::iterator_t<Base>>
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

template <typename URange, typename Fn, bool Const>
    requires detail::transform::constraints<URange, Fn>
class transform_sentinel
{
    using Base = detail::maybe_const<Const, URange>;

    std::ranges::sentinel_t<Base> end_ = std::ranges::sentinel_t<Base>();

    template <typename URange_, typename Fn_, bool Const_>
        requires detail::transform::constraints<URange, Fn>
    friend class transform_iterator;
    template <typename URange_, typename Fn_, bool Const_>
        requires detail::transform::constraints<URange, Fn>
    friend class transform_sentinel;

public:
    transform_sentinel() = default;

    constexpr explicit transform_sentinel(std::ranges::sentinel_t<Base> end) : end_(end) {}

    constexpr transform_sentinel(Fn, std::ranges::sentinel_t<Base> end) : end_(end) {}

    // Note: `i` should always be `transform_sentinel<false>`, but directly using
    // `transform_sentinel<false>` is ill-formed when `Const` is false
    // (see http://wg21.link/class.copy.ctor#5).
    constexpr transform_sentinel(transform_sentinel<URange, Fn, !Const> i)
        requires Const && std::convertible_to<std::ranges::sentinel_t<URange>, std::ranges::sentinel_t<Base>>
      : end_(std::move(i.end_))
    {}

    constexpr std::ranges::sentinel_t<Base> base() const { return end_; }

    template <bool OtherConst>
        requires std::sentinel_for<std::ranges::sentinel_t<Base>,
                                   std::ranges::iterator_t<detail::maybe_const<OtherConst, URange>>>
    friend constexpr bool operator==(transform_iterator<URange, Fn, OtherConst> const & x, transform_sentinel const & y)
    {
        return x.current_ == y.end_;
    }

    template <bool OtherConst>
        requires std::sized_sentinel_for<std::ranges::sentinel_t<Base>,
                                         std::ranges::iterator_t<detail::maybe_const<OtherConst, URange>>>
    friend constexpr std::ranges::range_difference_t<detail::maybe_const<OtherConst, URange>> operator-(
      transform_iterator<URange, Fn, OtherConst> const & x,
      transform_sentinel const &                         y)
    {
        return x.current_ - y.end_;
    }

    template <bool OtherConst>
        requires std::sized_sentinel_for<std::ranges::sentinel_t<Base>,
                                         std::ranges::iterator_t<detail::maybe_const<OtherConst, URange>>>
    friend constexpr std::ranges::range_difference_t<detail::maybe_const<OtherConst, URange>> operator-(
      transform_sentinel const &                         x,
      transform_iterator<URange, Fn, OtherConst> const & y)
    {
        return x.end_ - y.current_;
    }
};

inline constexpr auto transform_borrow =
  []<std::ranges::borrowed_range URange, std::copy_constructible Fn>(URange && urange, Fn fn)
    requires(detail::transform::constraints<URange, Fn>)
{
    using URangeNoCVRef = std::remove_cvref_t<URange>;
    static constexpr bool is_simple =
      std::is_const_v<std::remove_reference_t<URange>> || detail::simple_range<URangeNoCVRef>;

    using iterator_t = transform_iterator<URangeNoCVRef, Fn, is_simple>;
    using sentinel_t = decltype(detail::overloaded{
      [] [[noreturn]] (auto &&) -> transform_sentinel<URangeNoCVRef, Fn, is_simple> { /*unreachable*/ },
      []<std::ranges::common_range Rng> [[noreturn]] (Rng &&) -> iterator_t { /*unreachable*/ }}(urange));

    using const_iterator_t = decltype(detail::overloaded{
      [] [[noreturn]] (auto &&) -> std::nullptr_t { /*unreachable*/ },
      []<std::ranges::borrowed_range Rng> [[noreturn]] (Rng &&) -> transform_iterator<URangeNoCVRef, Fn, true>
      { /*unreachable*/ }}(std::as_const(urange)));

    using const_sentinel_t = decltype(detail::overloaded{
      [] [[noreturn]] (auto &&) -> std::nullptr_t { /*unreachable*/ },
      []<std::ranges::borrowed_range Rng> [[noreturn]] (Rng &&) -> transform_sentinel<URangeNoCVRef, Fn, true>
      { /*unreachable*/ },
      []<std::ranges::borrowed_range Rng> requires std::ranges::common_range<Rng> [[noreturn]] (Rng &&)
        ->const_iterator_t{/*unreachable*/}}(std::as_const(urange)));

    if constexpr (std::ranges::sized_range<URange>)
    {
        using BorrowingRad =
          borrowing_rad<iterator_t, sentinel_t, const_iterator_t, const_sentinel_t, borrowing_rad_kind::sized>;

        return BorrowingRad{
          iterator_t{fn, std::ranges::begin(urange)},
          sentinel_t{fn,   std::ranges::end(urange)},
          std::ranges::size(urange)
        };
    }
    else
    {
        using BorrowingRad =
          borrowing_rad<iterator_t, sentinel_t, const_iterator_t, const_sentinel_t, borrowing_rad_kind::unsized>;

        return BorrowingRad{
          iterator_t{fn, std::ranges::begin(urange)},
          sentinel_t{fn,   std::ranges::end(urange)}
        };
    }
};

inline constexpr auto transform_coro = []<adaptable_range URange, std::copy_constructible Fn>(URange && urange, Fn fn)
    requires(std::regular_invocable<Fn &, std::ranges::range_reference_t<URange>> &&
             detail::can_reference<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);

    // we need to create inner functor so that it can take by value
    return
      [](auto urange_, Fn fn) -> radr::generator<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>
    {
        for (auto && elem : urange_)
            co_yield fn(std::forward<decltype(elem)>(elem));
    }(std::move(urange), std::move(fn));
};

} // namespace radr

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto transform = detail::pipe_with_args_fn{transform_coro, transform_borrow};
} // namespace cpo
} // namespace radr::pipe
