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

#include "take.hpp"

namespace radr::detail
{

inline constexpr auto take_exactly_borrow =
  []<borrowed_mp_range URange>(URange && urange, range_size_t_or_size_t<URange> const n)
{
    if constexpr (std::ranges::sized_range<URange>)
        return take_borrow(borrowing_rad{urange}, n);
    else
        return take_borrow(borrowing_rad{urange, n}, n); // exact value for first size not important
};
} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto take_exactly = detail::pipe_with_args_fn{detail::take_coro, detail::take_exactly_borrow};
} // namespace cpo
} // namespace radr
