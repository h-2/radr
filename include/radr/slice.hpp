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
#include "drop.hpp"
#include "generator.hpp"
#include "tags.hpp"
#include "take.hpp"

namespace radr
{

inline constexpr auto slice_borrow =
  detail::overloaded{[]<std::ranges::borrowed_range URange>(URange && urange, size_t const start, size_t const end)
                     {
                         size_t const t = end >= start ? end - start : 0ull;
                         return take_borrow(drop_borrow(std::forward<URange>(urange), start), t);
                     },
                     []<subborrowable_range URange>(URange && urange, size_t const start, size_t const end)
                     { return subborrow(std::forward<URange>(urange), start, end); }};

inline constexpr auto slice_coro = []<movable_range URange>(URange && urange, size_t const start, size_t const end)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);
    size_t const t = end >= start ? end - start : 0ull;
    return take_coro(drop_coro(std::forward<URange>(urange), start), t);
};

} // namespace radr

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto slice = detail::pipe_with_args_fn{slice_coro, slice_borrow};
} // namespace cpo
} // namespace radr::pipe
