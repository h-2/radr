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
#include "detail/detail.hpp"
#include "detail/pipe.hpp"
#include "generator.hpp"
#include "tags.hpp"

namespace radr::custom
{

//=============================================================================
// tag_invoke overloads
//=============================================================================

template <std::ranges::borrowed_range URange>
    requires(std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
auto tag_invoke(subborrow_tag, URange && urange, size_t const start, size_t const end)
{
    using BorrowingRad = borrowing_rad<std::ranges::iterator_t<URange>,
                                       std::ranges::iterator_t<URange>,
                                       detail::const_it_or_nullptr_t<URange>,
                                       detail::const_it_or_nullptr_t<URange>,
                                       borrowing_rad_kind::sized>;
    size_t const e     = std::min<size_t>(end, std::ranges::size(urange));
    size_t const b     = std::min<size_t>(start, e);
    size_t const s     = e - b;

    return BorrowingRad{std::ranges::begin(urange) + b, std::ranges::begin(urange) + e, s};
}

template <std::ranges::borrowed_range URange>
    requires(std::ranges::contiguous_range<URange> && std::ranges::sized_range<URange>)
auto tag_invoke(subborrow_tag, URange && urange, size_t const start, size_t const end)
{
    size_t const e = std::min<size_t>(end, std::ranges::size(urange));
    size_t const b = std::min<size_t>(start, e);

    return std::span{std::ranges::data(urange) + b, std::ranges::data(urange) + e};
}

template <std::ranges::borrowed_range URange>
    requires(std::ranges::contiguous_range<URange> && std::ranges::sized_range<URange> &&
             std::same_as<std::ranges::range_reference_t<URange>, char const &>)
auto tag_invoke(subborrow_tag, URange && urange, size_t const start, size_t const end)
{
    size_t const e = std::min<size_t>(end, std::ranges::size(urange));
    size_t const b = std::min<size_t>(start, e);

    return std::string_view{std::ranges::data(urange) + b, std::ranges::data(urange) + e};
}

// TODO overloads for subrange and empty_view and iota_views that are borrowed
// technically, all RA+sized borrowed ranges from std could have overloads; but likely too much work

} // namespace radr::custom

namespace radr
{

//=============================================================================
// subborrowable_range concept
//=============================================================================

template <typename Range>
concept subborrowable_range = std::ranges::borrowed_range<Range> && std::ranges::forward_range<Range> && requires {
    {
        tag_invoke(custom::subborrow_tag{}, std::declval<Range>(), 0ull, 0ull)
    } -> std::ranges::borrowed_range;
};

//=============================================================================
// Wrapper function
//=============================================================================

inline constexpr auto subborrow = []<subborrowable_range URange>(URange && urange, size_t const start, size_t const end)
{ return tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), start, end); };

} // namespace radr
