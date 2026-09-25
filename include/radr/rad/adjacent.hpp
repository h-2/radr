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
/*!\brief Create a sliding tuple of fixed size N over the range.
 * \tparam N The size of the tuple (>= 1).
 * \param urange The underlying range.
 *
 * Requires C++23!
 *
 * ### Multi-pass adaptor
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
 *
 * ### Single-pass adaptor
 *
 * radr::adjacent cannot be created on single-pass ranges.
 */
template <ptrdiff_t N>
inline constexpr auto adjacent = detail::pipe_without_args_fn<void, decltype(detail::adjacent_borrow<N>)>{};
} // namespace cpo
} // namespace radr

#endif
