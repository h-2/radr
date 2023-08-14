// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <ranges>
#include <span>
#include <string_view>

#include "borrowing_rad.hpp"

namespace radr
{

inline constexpr auto borrow = []<std::ranges::borrowed_range URange>(URange && urange)
/*requires(!std::ranges::borrowed_range<std::remove_cvref_t<URange>> ||
             std::constructible_from<std::remove_cvref_t<URange>, URange>)*/
{
    using URangeNoCVRef = std::remove_cvref_t<URange>;

    // already a borrowed type
    if constexpr (std::ranges::borrowed_range<URangeNoCVRef> && std::constructible_from<URangeNoCVRef, URange>)
    {
        return urange; // NOOP
    }
    else if constexpr (std::ranges::contiguous_range<URange> && std::ranges::sized_range<URange>)
    {
        if constexpr (std::same_as<std::ranges::range_reference_t<URange>, char const &>)
        {
            return std::string_view{std::ranges::data(urange), std::ranges::data(urange) + std::ranges::size(urange)};
        }
        else
        {
            return std::span{std::forward<URange>(urange)};
        }
    }
    else
    {
        return borrowing_rad{std::forward<URange>(urange)};
    }
};

inline constexpr auto range_fwd = []<std::ranges::range Range>(Range && range) -> decltype(auto)
{
    if constexpr (std::is_lvalue_reference_v<Range>)
        return borrow(range);
    else
        return std::forward<Range>(range);
};

} // namespace radr

#define RADR_FWD(R) radr::range_fwd(std::forward<decltype(R)>(R))
