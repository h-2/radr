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

#include <memory>
#include <ranges>

#include "concepts.hpp"
#include "detail.hpp"
#include "generator.hpp"
#include "rad_interface.hpp"
#include "range_bounds.hpp"

/* TODO add version that stores offsets for RA+sized uranges
 * this avoids unique_ptr and RangeBounds
 *
 * TODO add collapsing constructor that avoids nesting cached_bounds_rad
 *
 */

namespace radr
{

template <rad_constraints URange, typename RangeBounds = decltype(range_bounds{std::declval<URange &>()})>
class cached_bounds_rad : public rad_interface<cached_bounds_rad<URange>>
{
    [[no_unique_address]] std::unique_ptr<URange> base_ = nullptr;
    [[no_unique_address]] RangeBounds             bounds;

    static constexpr bool const_iterable = std::ranges::forward_range<RangeBounds const &>;
    static constexpr bool simple         = detail::simple_range<RangeBounds>;

public:
    cached_bounds_rad()
        requires std::default_initializable<RangeBounds>
    = default;

    cached_bounds_rad(cached_bounds_rad &&)             = default;
    cached_bounds_rad & operator=(cached_bounds_rad &&) = default;

    cached_bounds_rad(cached_bounds_rad const & rhs)
        requires(std::constructible_from<URange, URange const &> && std::copyable<RangeBounds>)
    {
        if (rhs.base_ != nullptr)
        {
            base_.reset(new URange(*rhs.base_)); // deep copy of range
            bounds = RangeBounds{*base_};
        }
    }

    cached_bounds_rad & operator=(cached_bounds_rad const & rhs)
        requires(std::constructible_from<URange, URange const &> && std::copyable<RangeBounds>)
    {
        if (rhs.base_ == nullptr)
        {
            base_.reset(nullptr);
            bounds = rhs.bounds;
        }
        else
        {
            base_.reset(new URange(*rhs.base_)); // deep copy of range
            bounds = RangeBounds{*base_};
        }

        return *this;
    }

    constexpr explicit cached_bounds_rad(URange && base) : base_(new URange(std::move(base))), bounds{*base_} {}
    //TODO collapsing constructor

    constexpr cached_bounds_rad(URange && base, std::regular_invocable<URange &> auto cacher_fn) :
      base_(new URange(std::move(base))), bounds{cacher_fn(*base_)}
    {}
    //TODO collapsing constructor

    constexpr URange base() const &
        requires std::copy_constructible<URange>
    {
        return base_;
    }
    constexpr URange base() && { return std::move(base_); }

    constexpr auto begin()
        requires(!simple)
    {
        return bounds.begin();
    }

    constexpr auto begin() const
        requires const_iterable
    {
        return bounds.begin();
    }

    constexpr auto end()
        requires(!simple)
    {
        return bounds.end();
    }

    constexpr auto end() const
        requires const_iterable
    {
        return bounds.end();
    }

    constexpr auto size() const
        requires std::ranges::sized_range<RangeBounds const>
    {
        return bounds.size();
    }
};

template <class Range>
cached_bounds_rad(Range &&) -> cached_bounds_rad<std::remove_cvref_t<Range>>;

template <class Range, std::regular_invocable<std::remove_cvref_t<Range> &> CacherFn>
cached_bounds_rad(Range &&, CacherFn)
  -> cached_bounds_rad<std::remove_cvref_t<Range>, std::invoke_result_t<CacherFn, std::remove_cvref_t<Range> &>>;

} // namespace radr

template <class Range>
inline constexpr bool std::ranges::enable_view<radr::cached_bounds_rad<Range>> = std::ranges::view<Range>;

namespace radr::pipe
{

inline namespace cpo
{
// clang-format off
inline constexpr auto cached_bounds = detail::range_adaptor_closure_t{detail::overloaded{
    []<std::ranges::forward_range URange>(URange && urange)
    {
        static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);
        if constexpr (std::ranges::borrowed_range<URange>)
            return range_bounds{urange};
        else
            return cached_bounds_rad{std::move(urange)};
    },
    []<std::ranges::forward_range URange>(std::reference_wrapper<URange> const & urange)
    {
        return range_bounds{static_cast<URange &>(urange)};
    }
}};

// clang-format on

} // namespace cpo
} // namespace radr::pipe
