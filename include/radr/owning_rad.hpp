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
#include "detail/detail.hpp"
#include "generator.hpp"
#include "rad_interface.hpp"
#include "tags.hpp"

/* TODO add version that stores offsets for RA+sized uranges
 * this avoids unique_ptr and BorrowedRange
 * NO; THIS WILL BE SEPARATE
 *
 * TODO add collapsing constructor that avoids nesting owning_rad
 *
 */

namespace radr
{

template <unqualified_range URange, std::ranges::borrowed_range BorrowedRange>
class owning_rad : public rad_interface<owning_rad<URange, BorrowedRange>>
{
    [[no_unique_address]] std::unique_ptr<URange> base_ = nullptr;
    [[no_unique_address]] BorrowedRange           bounds;

    static constexpr bool const_iterable = std::ranges::forward_range<BorrowedRange const &>;
    static constexpr bool simple         = detail::simple_range<BorrowedRange>;

    template <unqualified_range URange_, std::ranges::borrowed_range BorrowedRange_>
    friend class owning_rad;

public:
    owning_rad()
        requires std::default_initializable<BorrowedRange>
    = default;

    owning_rad(owning_rad &&)             = default;
    owning_rad & operator=(owning_rad &&) = default;

    owning_rad(owning_rad const & rhs)
        requires(std::constructible_from<URange, URange const &> && std::copyable<BorrowedRange>)
    {
        if (rhs.base_ != nullptr)
        {
            // deep copy of range
            if (base_)
                *base_ = *rhs.base_;
            else
                base_.reset(new URange(*rhs.base_));
            // shallow copy of bounds
            bounds = BorrowedRange{*base_};
        }
    }

    owning_rad & operator=(owning_rad const & rhs)
        requires(std::constructible_from<URange, URange const &> && std::copyable<BorrowedRange>)
    {
        if (rhs.base_ == nullptr)
        {
            base_.reset(nullptr);
            bounds = rhs.bounds;
        }
        else
        {
            // deep copy of range
            if (base_)
                *base_ = *rhs.base_;
            else
                base_.reset(new URange(*rhs.base_));
            // shallow copy of bounds
            bounds = BorrowedRange{*base_};
        }

        return *this;
    }

    constexpr explicit owning_rad(URange && base) : base_(new URange(std::move(base))), bounds{*base_} {}

    template <typename Fn>
        requires std::regular_invocable<Fn &&, URange &>
    constexpr owning_rad(URange && base, Fn cacher_fn) :
      base_(new URange(std::move(base))), bounds{std::move(cacher_fn)(*base_)}
    {}

    //!\brief Collapsing constructor
    template <typename Fn, typename BorrowedRange_>
        requires std::regular_invocable<Fn &&, BorrowedRange_ &>
    constexpr owning_rad(owning_rad<URange, BorrowedRange_> && urange, Fn cacher_fn) :
      base_(std::move(urange.base_)), bounds{std::move(cacher_fn)(urange.bounds)}
    {}

    constexpr URange base() const &
        requires std::copy_constructible<URange>
    {
        return *base_;
    }
    constexpr URange base() && { return std::move(*base_); }

    constexpr auto begin()
        requires(!simple)
    {
        return std::ranges::begin(bounds);
    }

    constexpr auto begin() const
        requires const_iterable
    {
        return std::ranges::begin(bounds);
    }

    constexpr auto end()
        requires(!simple)
    {
        return std::ranges::end(bounds);
    }

    constexpr auto end() const
        requires const_iterable
    {
        return std::ranges::end(bounds);
    }

    constexpr auto size()
        requires std::ranges::sized_range<BorrowedRange>
    {
        return std::ranges::size(bounds);
    }

    constexpr auto size() const
        requires std::ranges::sized_range<BorrowedRange const>
    {
        return std::ranges::size(bounds);
    }
};

template <class Range>
owning_rad(Range &&)
  -> owning_rad<std::remove_cvref_t<Range>, decltype(borrowing_rad{std::declval<std::remove_cvref_t<Range> &>()})>;

template <class Range, class CacherFn>
    requires std::regular_invocable<CacherFn &&, std::remove_cvref_t<Range> &>
owning_rad(Range &&, CacherFn)
  -> owning_rad<std::remove_cvref_t<Range>, std::invoke_result_t<CacherFn &&, std::remove_cvref_t<Range> &>>;

template <class Range, class BorrowedRange_, class CacherFn>
    requires std::regular_invocable<CacherFn &&, std::remove_cvref_t<BorrowedRange_> &>
owning_rad(owning_rad<Range, BorrowedRange_> &&, CacherFn)
  -> owning_rad<std::remove_cvref_t<Range>, std::invoke_result_t<CacherFn &&, std::remove_cvref_t<BorrowedRange_> &>>;

} // namespace radr

//NOTE that this is typically not the case for our owning_rads; only when combined with std::views
template <class Range, class BorrowedRange>
inline constexpr bool std::ranges::enable_view<radr::owning_rad<Range, BorrowedRange>> =
  std::ranges::view<Range> && std::ranges::view<BorrowedRange>;
