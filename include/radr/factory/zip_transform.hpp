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

#include "radr/class/zip_container.hpp"
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
 * \param[in] other_ranges A pack of the other ranges; these may be rvalues of containers, too.
 * \details
 *
 * This factory is similar to `radr::zip(urange, other_ranges...) | radr::transform(fn)`, but \p fn is invoked as
 * `fn(a, b, …)` and no intermediate std::tuple is created.
 *
 * In contrast to the regular zip-family of adaptors and factories, the `*_transform`-variants do not require C++23.
 *
 * ## Comparison with other adaptors/factories
 *
 * |                                         | std::views::zip_transform                          | radr::zip_transform                                | radr::zip_with_transform                                 |
 * |-----------------------------------------|:--------------------------------------------------:|:--------------------------------------------------:|:--------------------------------------------------------:|
 * | minimum number of ranges                |                         0                          |                         1                          |                            1                             |
 * | lvalue of container for \p urange       |                        yes                         |                  std::ref-wrapped                  |                     std::ref-wrapped                     |
 * | lvalue of container for \p other_ranges |                        yes                         |                  std::ref-wrapped                  |                     std::ref-wrapped                     |
 * | rvalue of container for \p urange       |                        yes                         |                        yes                         |                           yes                            |
 * | rvalue of container for \p other_ranges |                        yes                         |                        yes                         |                            no                            |
 * | direct ("factory") call pattern         | `s::v::zip_transform(fn, urange, other_ranges...)` | `radr::zip_transform(fn, urange, other_ranges...)` | `radr::zip_with_transform(urange, fn, other_ranges...)`  |
 * | pipe ("adaptor") call pattern           |                         no                         |                         no                         | `urange | radr::zip_with_transform(fn, other_ranges...)` |
 * | closure ("stored") call pattern         |                         no                         |                         no                         | `radr::zip_with_transform(fn, other_ranges...)(urange)`  |
 *
 * See radr::zip_with_transform for the corresponding adaptor object, and for the full documentation of the
 * requirements and which concepts are preserved.
 *
 * Note the difference in argument order:
 *
 * ```cpp
 * radr::zip_transform(fn, std::ref(vec), std::ref(other));        // functor first, like std::views::zip_transform
 * radr::zip_with_transform(std::ref(vec), fn, std::ref(other));   // range first, because it can also be piped
 * ```
 *
 * ### Owning and borrowing
 *
 * Like radr::zip (and unlike radr::zip_with_transform), this factory accepts rvalues of containers in *every*
 * position:
 *   * If all arguments are borrowed (or std::ref-wrapped), the result is a radr::borrowing_rad and is exactly
 *     what radr::zip_with_transform returns for the same arguments.
 *   * Otherwise the result is a radr::zip_container with radr::zip_policy_transform that owns every argument.
 *
 * ### Single-pass ranges
 *
 * Like radr::zip, this factory rejects single-pass ranges. There is no `zip_transform_sp` object; use
 * radr::zip_with_transform instead, which returns a radr::generator for single-pass input.
 */
inline constexpr auto zip_transform =
  []<typename Fn, typename URange, typename... OtherRanges>(Fn && fn, URange && urange, OtherRanges &&... others)
{
    static_assert(std::ranges::input_range<std::remove_reference_t<URange>> ||
                    ref_wrapped_mp_range<std::remove_cvref_t<URange>>,
                  "The second argument of radr::zip_transform needs to be a range; note that the functor comes "
                  "first here, but second in radr::zip_with_transform.");

    if constexpr (safe_indirect_mp_range<URange> && (safe_indirect_mp_range<OtherRanges> && ...))
    {
        // all inputs borrowed → identical to the adaptor, which returns a radr::borrowing_rad
        return zip_with_transform(std::forward<URange>(urange),
                                  std::forward<Fn>(fn),
                                  std::forward<OtherRanges>(others)...);
    }
    else if constexpr ((mp_range<URange> || ref_wrapped_mp_range<URange>)&&((mp_range<OtherRanges> ||
                                                                             ref_wrapped_mp_range<OtherRanges>)&&...))
    {
        static_assert(!container_lvalue<URange> && (!container_lvalue<OtherRanges> && ...),
                      "Do not pass lvalues of containers to radr::zip_transform. "
                      "To store copies, pass copies; to store references, wrap inputs in std::ref().");

        using fn_t = std::remove_cvref_t<Fn>;

        /* the stored ranges are what RADR_FWD() produces, i.e. std::ref-wrapped ones become borrowing_rad */
        using u_t = std::remove_cvref_t<decltype(RADR_FWD(urange))>;

        static_assert(
          zip_policy_transform_constraints<fn_t,
                                           iterator_t<u_t>,
                                           iterator_t<std::remove_cvref_t<decltype(RADR_FWD(others))>>...> &&
            zip_policy_transform_constraints<fn_t,
                                             const_iterator_t<u_t>,
                                             const_iterator_t<std::remove_cvref_t<decltype(RADR_FWD(others))>>...>,
          "The constraints for radr::zip_transform's functor are not met.");

        return zip_container{zip_policy_transform<fn_t>{
                               detail::semiregular_box<fn_t>{std::in_place, std::forward<Fn>(fn)}},
                             RADR_FWD(urange),
                             RADR_FWD(others)...};
    }
    else
    {
        static_assert(std::integral<URange> /* always false */,
                      "radr::zip_transform does not accept single-pass ranges; use radr::zip_with_transform, "
                      "which returns a radr::generator for single-pass input.");
    }
};
} // namespace cpo
} // namespace radr
