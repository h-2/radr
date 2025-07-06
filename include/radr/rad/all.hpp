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

#include "radr/custom/subborrow.hpp"
#include "radr/detail/pipe.hpp"
#include "radr/rad/to_single_pass.hpp"

namespace radr
{

inline namespace cpo
{
/*!\brief A NOOP range adaptor provided for compatibility.
 * \param urange The underlying range.
 *
 * This range adaptor is semantically a NOOP, i.e. the returned range has the same elements
 * and generally satisfies the same concepts. However, the actual range type will be different
 * (and may be customised).
 *
 * ### Multi-pass adaptor
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *
 * Calls radr::borrow on \p urange which may result in a customised range type being returned.
 *
 * Unless customised otherwise, preserves all range concepts and is transparant (preserves iterator and
 * sentinel types).
 *
 * ### Single-pass adaptor
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *
 * Returns the range as-is.
 */
#ifndef RADR_ALL_NO_DEPRECATED
[[deprecated(
  "radr::all is provided as a (mostly) NOOP adaptor. You probably want radr::borrow instead.\n"
  "You can silence this warning by defining RADR_ALL_NO_DEPRECATED.")]]
#endif
inline constexpr auto all = detail::pipe_without_args_fn<decltype(detail::to_single_pass_coro), decltype(borrow)>{};
} // namespace cpo
} // namespace radr
