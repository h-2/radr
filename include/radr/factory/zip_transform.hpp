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

#include <ranges>
#include <utility>

#include "radr/concepts.hpp"
#include "radr/rad/zip_with_transform.hpp"

namespace radr
{

inline namespace cpo
{
/*!\brief Zips ranges and applies an invocable to every set of corresponding elements.
 * \tparam Fn Type of \p fn.
 * \tparam URange Type of \p urange.
 * \tparam OtherRanges Types of \p other_ranges.
 * \param[in] fn The invocable to apply; it is called with N arguments, not with a tuple.
 * \param[in] urange The first range; may be an rvalue of a container.
 * \param[in] other_ranges A pack of the other ranges; must be borrowed or wrapped in std::ref or std::cref.
 * \details
 *
 * This factory is similar to `radr::zip(urange, other_ranges...) | radr::transform(fn)`, but \p fn is invoked as
 * `fn(a, b, …)` and no intermediate std::tuple is created.
 *
 * In contrast to the regular zip-family of adaptors and factories, the `*_transform`-variants do not require C++23.
 *
 * ## Comparison with other adaptors
 *
 * |                                         | std::views::zip_transform                          | radr::zip_transform                                | radr::zip_with_transform                                 |
 * |-----------------------------------------|:--------------------------------------------------:|:--------------------------------------------------:|:--------------------------------------------------------:|
 * | minimum number of ranges                |                         0                          |                         1                          |                            1                             |
 * | lvalue of container for \p urange       |                        yes                         |                  std::ref-wrapped                  |                     std::ref-wrapped                     |
 * | lvalue of container for \p other_ranges |                        yes                         |                  std::ref-wrapped                  |                     std::ref-wrapped                     |
 * | rvalue of container for \p urange       |                        yes                         |                        yes                         |                           yes                            |
 * | rvalue of container for \p other_ranges |                        yes                         |                     no (TODO)                      |                            no                            |
 * | direct ("factory") call pattern         | `s::v::zip_transform(fn, urange, other_ranges...)` | `radr::zip_transform(fn, urange, other_ranges...)` | `radr::zip_with_transform(urange, fn, other_ranges...)`  |
 * | pipe ("adaptor") call pattern           |                         no                         |                         no                         | `urange | radr::zip_with_transform(fn, other_ranges...)` |
 * | closure ("stored") call pattern         |                         no                         |                         no                         | `radr::zip_with_transform(fn, other_ranges...)(urange)`  |
 *
 * See radr::zip_with_transform for the corresponding adaptor object, and for the full documentation of the
 * requirements and which concepts are preserved.
 *
 * Currently, the only difference between the two is the argument order:
 *
 * ```cpp
 * radr::zip_transform(fn, std::ref(vec), std::ref(other));        // functor first, like std::views::zip_transform
 * radr::zip_with_transform(std::ref(vec), fn, std::ref(other));   // range first, because it can also be piped
 * ```
 *
 * ### Current limitations
 *
 * This factory currently only forwards to radr::zip_with_transform. In contrast to radr::zip, it inherits that
 * adaptor's restrictions:
 *   * Only \p urange may be an rvalue of a container; every type in \p OtherRanges needs to model
 *     `radr::safe_indirect_mp_range` (wrap containers in `std::ref()`).
 *
 * ### Single-pass ranges
 *
 * Unlike radr::zip (which delegates to radr::zip_sp), this factory also accepts a single-pass \p urange and then
 * returns a radr::generator; there is no separate `zip_transform_sp` object.
 */
inline constexpr auto zip_transform =
  []<typename Fn, typename URange, typename... OtherRanges>(Fn && fn, URange && urange, OtherRanges &&... others)
{
    static_assert(std::ranges::input_range<std::remove_reference_t<URange>> ||
                    ref_wrapped_mp_range<std::remove_cvref_t<URange>>,
                  "The second argument of radr::zip_transform needs to be a range; note that the functor comes "
                  "first here, but second in radr::zip_with_transform.");

    return zip_with_transform(std::forward<URange>(urange), std::forward<Fn>(fn), std::forward<OtherRanges>(others)...);
};
} // namespace cpo
} // namespace radr
