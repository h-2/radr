// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023-2026 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstddef>

#include "radr/detail/adjacent_iterator.hpp"
#include "radr/version.hpp"

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include <algorithm>
#    include <array>
#    include <ranges>

#    include "radr/class/zip_container.hpp"
#    include "radr/concepts.hpp"
#    include "radr/detail/pipe.hpp"

namespace radr
{

inline namespace cpo
{
/*!\brief radr::adjacent<K>(urange)
 * \tparam K The size of the tuple (>= 1).
 * \param urange The underlying range.
 *
 * Turns a range into a range of overlapping tuples, each of size N.
 *
 * Requires C++23.
 *
 * Be aware of the following similar adaptors:
 *
 * |                     |  parameter        | element-type  |
 * |---------------------|-------------------|---------------|
 * | radr::adjacent      | compile-time      | tuple         |
 * | radr::slide_counted | compile-time      | range         |
 * | radr::slide         | runtime           | range         |
 *
 * ## Multi-pass adaptor
 *
 * Requirements on \p urange :
 *   * radr::mp_range
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::common_range
 *   * radr::constant_range
 *   * radr::mutable_range
 *   * radr::infinite_mp_range
 *
 * ### Notable differences to std::views::adjacent
 *
 * For underlying ranges that model radr::safely_indexable_range, this adaptor's iterator stores only one underlying
 * iterator.
 *
 * ### Performance
 *
 * Can be faster than std::views::adjacent, radr::slide_counted and radr::slide on bidirectional ranges.
 *
 * Can be faster than std::views::adjacent on random access ranges, especially ones that are
 * not contiguous.
 *
 * ## Single-pass adaptor
 *
 * radr::adjacent cannot be created on single-pass ranges.
 */
template <ptrdiff_t K>
inline constexpr auto adjacent = detail::pipe_without_args_fn<void, decltype(detail::adjacent_borrow<K>)>{};
} // namespace cpo
} // namespace radr

#endif
