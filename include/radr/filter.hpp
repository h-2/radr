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

#include "cached_bounds.hpp"
#include "concepts.hpp"
#include "copyable_box.hpp"
#include "detail.hpp"
#include "generator.hpp"
#include "rad_interface.hpp"
#include "range_ref.hpp"

namespace radr
{

template <rad_constraints URange, std::indirect_unary_predicate<std::ranges::iterator_t<URange>> Pred>
    requires std::is_object_v<Pred>
class filter_rad_nc : public rad_interface<filter_rad_nc<URange, Pred>>
{
    [[no_unique_address]] URange             base_ = URange();
    [[no_unique_address]] copyable_box<Pred> pred_;

    template <bool>
    class iterator;
    template <bool>
    class sentinel;

    static constexpr bool const_iterable = requires {
        requires std::ranges::forward_range<URange const>;
        requires std::indirect_unary_predicate<Pred const, std::ranges::iterator_t<URange const>>;
    };
    static constexpr bool simple = detail::simple_range<URange> && const_iterable;

public:
    filter_rad_nc()
        requires std::default_initializable<URange> && std::default_initializable<Pred>
    = default;

    constexpr explicit filter_rad_nc(URange && base, Pred pred) :
      base_(std::move(base)), pred_(std::in_place, std::move(pred))
    {}

    template <class Vp = URange>
    constexpr URange base() const &
        requires std::copy_constructible<Vp>
    {
        return base_;
    }
    constexpr URange base() && { return std::move(base_); }

    constexpr Pred const & pred() const { return *pred_; }

    constexpr iterator<false> begin()
        requires(!simple)
    {
        assert(pred_.has_value());
        return {*this, std::ranges::find_if(base_, std::ref(*pred_))};
    }

    constexpr iterator<true> begin() const
        requires(const_iterable)
    {
        assert(pred_.has_value());
        return {*this, std::ranges::find_if(base_, std::cref(*pred_))};
    }

    constexpr auto end()
        requires(!simple)
    {
        if constexpr (std::ranges::common_range<URange>)
            return iterator<false>{*this, std::ranges::end(base_)};
        else
            return sentinel<false>{*this};
    }

    constexpr auto end() const
        requires(const_iterable)
    {
        if constexpr (std::ranges::common_range<URange const>)
            return iterator<true>{*this, std::ranges::end(base_)};
        else
            return sentinel<true>{*this};
    }
};

template <class Range, class Pred>
filter_rad_nc(Range &&, Pred) -> filter_rad_nc<std::remove_cvref_t<Range>, Pred>;

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

template <rad_constraints URange, std::indirect_unary_predicate<std::ranges::iterator_t<URange>> Pred>
    requires std::is_object_v<Pred>
template <bool Const>
class filter_rad_nc<URange, Pred>::iterator : public filter_iterator_category<URange>
{
    using Parent  = detail::maybe_const<Const, filter_rad_nc>;
    using URangeC = detail::maybe_const<Const, URange>;

    template <bool>
    friend class filter_rad_nc<URange, Pred>::iterator;

    template <bool>
    friend class filter_rad_nc<URange, Pred>::sentinel;

public:
    [[no_unique_address]] Parent *                         parent_  = nullptr;
    [[no_unique_address]] std::ranges::iterator_t<URangeC> current_ = std::ranges::iterator_t<URangeC>();

    // clang-format off
    using iterator_concept =
        std::conditional_t<std::ranges::bidirectional_range<URangeC>, std::bidirectional_iterator_tag,
        std::conditional_t<std::ranges::forward_range<URangeC>,       std::forward_iterator_tag,
                           /* else */                                 std::input_iterator_tag>>;
    // clang-format on

    // using iterator_category = inherited;
    using value_type      = std::ranges::range_value_t<URangeC>;
    using difference_type = std::ranges::range_difference_t<URangeC>;

    iterator()
        requires std::default_initializable<std::ranges::iterator_t<URangeC>>
    = default;

    constexpr iterator(Parent & parent, std::ranges::iterator_t<URangeC> current) :
      parent_(std::addressof(parent)), current_(std::move(current))
    {}

    // Note: `i` should always be `iterator<false>`, but directly using
    // `iterator<false>` is ill-formed when `Const` is false
    // (see http://wg21.link/class.copy.ctor#5).
    constexpr iterator(iterator<!Const> i)
        requires(Const && std::convertible_to<std::ranges::iterator_t<URange>, std::ranges::iterator_t<URangeC>>)
      : parent_(i.parent_), current_(std::move(i.current_))
    {}

    constexpr std::ranges::iterator_t<URangeC> const & base() const & noexcept { return current_; }
    constexpr std::ranges::iterator_t<URangeC>         base() && { return std::move(current_); }

    constexpr std::ranges::range_reference_t<URangeC> operator*() const { return *current_; }
    constexpr std::ranges::iterator_t<URangeC>        operator->() const
        requires detail::has_arrow<std::ranges::iterator_t<URangeC>> && std::copyable<std::ranges::iterator_t<URangeC>>
    {
        return current_;
    }

    constexpr iterator & operator++()
    {
        current_ =
          std::ranges::find_if(std::move(++current_), std::ranges::end(parent_->base_), std::ref(*parent_->pred_));
        return *this;
    }
    constexpr void     operator++(int) { ++*this; }
    constexpr iterator operator++(int)
        requires std::ranges::forward_range<URangeC>
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr iterator & operator--()
        requires std::ranges::bidirectional_range<URangeC>
    {
        do
        {
            --current_;
        }
        while (!std::invoke(*parent_->pred_, *current_));
        return *this;
    }
    constexpr iterator operator--(int)
        requires std::ranges::bidirectional_range<URangeC>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    friend constexpr bool operator==(iterator const & x, iterator const & y)
        requires std::equality_comparable<std::ranges::iterator_t<URangeC>>
    {
        return x.current_ == y.current_;
    }

    friend constexpr std::ranges::range_rvalue_reference_t<URangeC> iter_move(iterator const & it) noexcept(
      noexcept(std::ranges::iter_move(it.current_)))
    {
        return std::ranges::iter_move(it.current_);
    }

    friend constexpr void iter_swap(iterator const & x, iterator const & y) noexcept(
      noexcept(std::ranges::iter_swap(x.current_, y.current_)))
        requires std::indirectly_swappable<std::ranges::iterator_t<URangeC>>
    {
        return std::ranges::iter_swap(x.current_, y.current_);
    }
};

template <rad_constraints URange, std::indirect_unary_predicate<std::ranges::iterator_t<URange>> Pred>
    requires std::is_object_v<Pred>
template <bool Const>
class filter_rad_nc<URange, Pred>::sentinel
{
    using Parent  = detail::maybe_const<Const, filter_rad_nc>;
    using URangeC = detail::maybe_const<Const, URange>;

    template <bool>
    friend class filter_rad_nc<URange, Pred>::iterator;

    template <bool>
    friend class filter_rad_nc<URange, Pred>::sentinel;

public:
    std::ranges::sentinel_t<URangeC> end_ = std::ranges::sentinel_t<URangeC>();

    sentinel() = default;

    constexpr explicit sentinel(Parent & parent) : end_(std::ranges::end(parent.base_)) {}

    constexpr std::ranges::sentinel_t<URangeC> base() const { return end_; }

    template <bool OtherConst>
        requires std::sentinel_for<std::ranges::sentinel_t<URangeC>,
                                   std::ranges::iterator_t<detail::maybe_const<OtherConst, URange>>>
    friend constexpr bool operator==(iterator<OtherConst> const & x, sentinel const & y)
    {
        return x.current_ == y.end_;
    }
};

inline constexpr auto filter_coro =
  []<adaptable_range URange, std::indirect_unary_predicate<std::ranges::iterator_t<URange>> Fn>(URange && urange, Fn fn)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);

    // we need to create inner functor so that it can take by value
    return [](auto urange_,
              Fn   fn) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        for (auto && elem : urange_)
            if (fn(elem))
                co_yield elem;
    }(std::move(urange), std::move(fn));
};

} // namespace radr

namespace radr::detail
{

template <bool caching>
struct filter_fn
{
    template <std::ranges::input_range URange, class Fn>
    [[nodiscard]] constexpr auto operator()(URange && range, Fn && f) const
      noexcept(noexcept(filter_coro(std::forward<URange>(range), std::forward<Fn>(f))))
    {
        static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);
        return filter_coro(std::forward<URange>(range), std::forward<Fn>(f));
    }

    template <std::ranges::forward_range URange, class Fn>
        requires(!caching)
    [[nodiscard]] constexpr auto operator()(URange && range, Fn && f) const
      noexcept(noexcept(filter_rad_nc(std::forward<URange>(range), std::forward<Fn>(f))))
    {
        static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);
        return filter_rad_nc(std::forward<URange>(range), std::forward<Fn>(f));
    }

    template <std::ranges::forward_range URange, class Fn>
        requires(caching)
    [[nodiscard]] constexpr auto operator()(URange && range, Fn && f) const
      noexcept(noexcept(filter_rad_nc(std::forward<URange>(range), std::forward<Fn>(f)) | pipe::cached_bounds))
    {
        static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);
        return filter_rad_nc(std::forward<URange>(range), std::forward<Fn>(f)) | pipe::cached_bounds;
    }

    template <class URange, class Fn>
    [[nodiscard]] constexpr auto operator()(std::reference_wrapper<URange> const & range, Fn && f) const
      noexcept(noexcept(operator()(range_ref{range}, std::forward<Fn>(f))))
        -> decltype(operator()(range_ref{range}, std::forward<Fn>(f)))
    {
        return operator()(range_ref{range}, std::forward<Fn>(f));
    }

    template <class Fn>
        requires std::constructible_from<std::decay_t<Fn>, Fn>
    [[nodiscard]] constexpr auto operator()(Fn && f) const
      noexcept(std::is_nothrow_constructible_v<std::decay_t<Fn>, Fn>)
    {
        return range_adaptor_closure_t{detail::bind_back(*this, std::forward<Fn>(f))};
    }
};

} // namespace radr::detail

namespace radr::pipe
{
inline namespace cpo
{
inline constexpr auto filter_nc = detail::filter_fn<false>{};
inline constexpr auto filter    = detail::filter_fn<true>{};
} // namespace cpo
} // namespace radr::pipe
