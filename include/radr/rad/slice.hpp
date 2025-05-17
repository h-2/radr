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

#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "drop.hpp"
#include "take.hpp"

namespace radr::detail
{

inline constexpr auto slice_borrow =
  []<std::ranges::borrowed_range URange>(URange && urange, size_t const start, size_t const end)
{
    if constexpr (std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
    {
        return subborrow(std::forward<URange>(urange), start, end);
    }
    else
    {
        size_t const t = end >= start ? end - start : 0ull;
        return take_borrow(drop_borrow(std::forward<URange>(urange), start), t);
    }
};

inline constexpr auto slice_coro =
  []<std::ranges::input_range URange>(URange && urange, size_t const start, size_t const end)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    size_t const t = end >= start ? end - start : 0ull;
    return take_coro(drop_coro(std::forward<URange>(urange), start), t);
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto slice = detail::pipe_with_args_fn{detail::slice_coro, detail::slice_borrow};
} // namespace cpo
} // namespace radr
