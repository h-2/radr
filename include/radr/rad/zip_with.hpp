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

#include "radr/concepts.hpp"
#include "radr/detail/detail.hpp"
#include "radr/version.hpp"

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include "radr/detail/pipe.hpp"
#    include "radr/factory/zip.hpp"

namespace radr::detail
{

inline constexpr auto zip_with_borrow =
  []<typename URange, typename... OtherRanges>(URange && urange, OtherRanges &&... others)
{
    static_assert(borrowed_mp_range<URange>,
                  "The constraints for radr::zip_with's underlying (primary) range are not met.");

    static_assert((safe_indirect_mp_range<OtherRanges> && ...),
                  "All ranges passed to radr::zip_with after the first need to be \n"
                  "  1) multi-pass ranges; to create a single-pass adaptor, wrap the first argument into "
                  "radr::to_single_pass.\n"
                  "  2) safe/explicit indirections; did you forget to wrap a container in std::ref() or std::cref()?");

    return zip_with_borrow_impl<zip_iterator_kind::adaptor>(zip_deref{},
                                                            radr::borrow(std::forward<URange>(urange)),
                                                            radr::borrow(std::forward<OtherRanges>(others))...);
};

// see the section on "Call patterns" in the documentation below.
struct zip_with_fn
{
    template <class... Args>
    [[nodiscard]] constexpr auto operator()(Args &&... args) const
      noexcept((std::is_nothrow_constructible_v<std::decay_t<Args>, Args> && ...))
    {
        static_assert((std::constructible_from<std::decay_t<Args>, Args> && ...));
        static_assert((safe_indirect_mp_range<Args> && ...), RADR_ASSERTSTRING_RVALUE);

        return range_adaptor_closure_t{
          detail::bind_back(pipe_with_args_fn<decltype(zip_sp), decltype(zip_with_borrow), true>{},
                            std::forward<Args>(args)...)};
    }

    [[nodiscard]] constexpr auto operator()() const noexcept
    {
        return pipe_without_args_fn<decltype(zip_sp), decltype(zip_with_borrow)>{};
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Zips ranges with the given one.
 * \tparam URange Type of \p urange.
 * \tparam OtherRanges Types of \p other_ranges.
 * \param[in] urange The underlying range.
 * \param[in] other_ranges A pack of the other ranges; each wrapped in std::ref or std::cref.
 * \details
 *
 * Zip other ranges with a given one.
 *
 * ## Comparison with other adaptors/factories
 *
 * |                                         |  std::views::zip                     |  radr::zip                           |  radr::zip_with                            |
 * |-----------------------------------------|:------------------------------------:|:------------------------------------:|:------------------------------------------:|
 * | minimum number of ranges                |   0                                  |   1                                  |                  1                         |
 * | lvalue of container for \p urange       | yes                                  | std::ref-wrapped                     |    std::ref-wrapped                        |
 * | lvalue of container for \p other_ranges | yes                                  | std::ref-wrapped                     |    std::ref-wrapped                        |
 * | rvalue of container for \p urange       | yes                                  | yes                                  |      yes                                   |
 * | rvalue of container for \p other_ranges | yes                                  | yes                                  |      no                                    |
 * | direct ("factory") call pattern         | `s::v::zip(urange, other_ranges...)` | `radr::zip(urange, other_ranges...)` | *no* (see below)                           |
 * | pipe ("adaptor") call pattern           | no                                   | no                                   | `urange | radr::zip_with(other_ranges...)` |
 * | closure ("stored") call pattern         | no                                   | no                                   | `radr::zip_with(other_ranges...)(urange)`  |
 *
 * In this library there are two objects corresponding to std::views::zip:
 *   * radr::zip: a factory object that is very similar to std::views::zip.
 *   * radr::zip_with: a range adaptor that allows the pipe call pattern, but does not allow container rvalues in \p other_ranges.
 *
 * For radr::zip_with, **the direct call pattern is not supported**. The respective syntax always returns a closure (i.e. the first part of the closure call pattern).
 * Use one of the other syntaxes, or use `radr::zip` instead of `radr::zip_with`.
 *
 * Zipping a single range is supported, but empty parentheses must be given, e.g. `urange | radr::zip_with()`.
 *
 * ## Multi-pass adaptor
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * Each type in `OtherRanges` needs to model `radr::safe_indirect_mp_range` (wrap containers in `std::ref()`).
 *
 * This adaptor preserves, if all underlying ranges provide it:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::constant_range
 *   * radr::mutable_range (see below)
 *
 * Additionally, it models std::ranges::sized_range if all underlying ranges model radr::safely_indexable_range and
 * at least one range models std::ranges::sized_range.
 *
 * It models radr::common_range, if one of the following conditions is met:
 *   * All underlying ranges model radr::common_range and at least one does **not** model std::ranges::bidirectional_range.
 *   * Or: all underlying ranges model radr::safely_indexable_range and at least one range models std::ranges::sized_range.
 *
 * If you want to zip more than one container, use the `radr::zip` factory instead.
 *
 * ### Notable difference to `std::views::zip`
 *
 *  * You can pipe into `radr::zip_with`, e.g. `std::string_view{"ABC} | radr::zip_with(std::string_view{"DEF"})`.
 *  * At least one range needs to be given.
 *
 * ## Single-pass adaptor
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *   * Each type in `OtherRanges` needs to model `std::ranges::input_range` (wrap containers in `std::ref()`).
 *
 */
inline constexpr detail::zip_with_fn zip_with{};
} // namespace cpo
} // namespace radr

#endif
