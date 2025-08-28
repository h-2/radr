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
 * |                                            |  std::views::zip |  radr::zip       |  radr::zip_with      |
 * |--------------------------------------------|:----------------:|:----------------:|:--------------------:|
 * | minimum arguments                          |   0              |   1              |                  1   |
 * | pipe into                                  |    no            |    no            |               yes    |
 * | lvalues of containers allowed (first arg)  | yes              | std::ref-wrapped |    std::ref-wrapped  |
 * | lvalues of containers allowed (other args) | yes              | std::ref-wrapped |    std::ref-wrapped  |
 * | rvalues of containers allowed (first arg)  | yes              | yes              |      yes             |
 * | rvalues of containers allowed (other args) | yes              | yes              |      no              |
 *
 * Use `radr::zip` if you need to capture more than one container by rvalue.
 *
 * Use `radr::zip_with` if you need pipe-support.
 *
 * ## Call pattern
 *
 * Typically, the following call patterns are identical for range adaptors:
 *   1. `urange | radr::foo(param1, param2);`
 *   2. `radr::foo(urange, param1, param2);`
 *   3. `radr::foo(param1, param2)(urange);`
 *
 * For this adaptor, **the second pattern has different semantics**, it always returns a closure.
 * Use one of the other syntaxes, or use `radr::zip` instead of `radr::zip_with`.
 *
 * Zipping a single range is supported, but empty parantheses must be given, e.g. `urange | radr::zip_with()`.
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
 *   * All underlying ranges model radr::common_range and it least one does **not** model std::ranges::bidirectional_range.
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
