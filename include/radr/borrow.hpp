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
#include "subborrow.hpp"

namespace radr
{

//TODO nothing uses this at the moment

inline constexpr auto range_fwd = []<std::ranges::range Range>(Range && range) -> decltype(auto)
{
    if constexpr (std::is_lvalue_reference_v<Range>)
        return subborrow(range);
    else
        return std::forward<Range>(range);
};

} // namespace radr

#define RADRFWD(R) radr::range_fwd(std::forward<decltype(R)>(R))
