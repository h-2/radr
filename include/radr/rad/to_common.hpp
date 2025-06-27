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

#include "../concepts.hpp"
#include "../detail/pipe.hpp"
#include "radr/custom/find_common_end.hpp"
#include "radr/custom/subborrow.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{
inline constexpr auto to_common_borrow = []<borrowed_mp_range URange>(URange && urange)
{
    if constexpr (common_range<URange>)
    {
        return borrow(urange);
    }
    else
    {
        return subborrow(urange,
                         radr::begin(urange),
                         radr::find_common_end(radr::begin(urange), radr::end(urange)),
                         size_or_not(urange));
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Turn a non-common range into a common range by finding the end-iterator (linearly).
 * \param urange The underlying range.
 * \details
 *
 * This range adaptor only affects the concepts modelled; the range elements are the same.
 *
 * ## Multi-pass adaptor
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *
 * For ranges that already model radr::common_range:
 *   * returns an identity/NOOP adaptor.
 *
 * This adaptor preserves:
 *   * all underlying range concepts
 *   * radr::iterator_t and radr::const_iterator_t
 *
 * It does not preserve:
 *   * radr::sentinel_t and radr::const_sentinel_t (because they become the respective iterator types).
 *
 * The end is determined by invoking the radr::find_common_end customisation point.
 * Construction of the adaptor is in O(n), because, by default,
 * **the end is searched linearly from the beginning.**
 * Customisations may provider faster implementations.
 *
 * ### Notable differences to std::views::common
 *
 * |               | std::views::common                          |              radr::to_common  |
 * |---------------|---------------------------------------------|-------------------------------|
 * | construction  | O(1)                                        | O(n)                          |
 * | `.begin()`    | O(1)                                        | O(1)                          |
 * | itarator_t    | different from underlying                   | same as underlying            |
 * | iteration     | slower than underlying                      | no overhead                   |
 *
 * Like with our other adaptors, you potentially pay a higher up-front cost for lower overhead later on and
 * simpler types.
 *
 * ### Example
 *
 * ```cpp
 * std::string_view str = "foo bar";
 *
 * auto rad1 = str | radr::take_while([] (char c) { return c != ' ';});
 * // the type of rad1 is borrowing_rad<…take_while_sentinel<…>…>
 *
 * auto rad2 = rad1 | radr::to_common;
 * // the type of rad2 is just std::string_view again
 * ```
 *
 * This is not possible with std::views::common, because the range type gets even more complicated.
 *
 * ## Single-pass adaptor
 *
 * This adaptor cannot be created on single-pass ranges.
 *
 */

inline constexpr auto to_common = detail::pipe_without_args_fn<void, decltype(detail::to_common_borrow)>{};

[[deprecated(
  "Use radr::to_common instead of std::views::common, but see the docs regarding semantic "
  "differences.")]] inline constexpr auto common = to_common;

} // namespace cpo
} // namespace radr
