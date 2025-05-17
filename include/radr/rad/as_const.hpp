// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023-2025 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <ranges>

#include "../concepts.hpp"
#include "../custom/subborrow.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"

namespace radr::detail
{

inline constexpr auto as_const_borrow = []<const_borrowable_range URange>(URange && urange)
{
    if constexpr (std::ranges::sized_range<URange>)
    {
        return radr::subborrow(urange, radr::cbegin(urange), radr::cend(urange), std::ranges::size(urange));
    }
    else
    {
        return radr::subborrow(urange, radr::cbegin(urange), radr::cend(urange));
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto as_const = detail::pipe_without_args_fn<void, decltype(detail::as_const_borrow)>{};
} // namespace cpo
} // namespace radr
