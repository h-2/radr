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

#include "radr/concepts.hpp"
#include "radr/custom/subborrow.hpp"
#include "radr/detail/zip_borrow.hpp"
#include "radr/generator.hpp"
#include "radr/range_access.hpp"
#include "radr/version.hpp"

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include "radr/class/zip_rng.hpp"

namespace radr
{

inline namespace cpo
{

/*!\brief Zips multi-pass ranges.
 * \tparam URanges Type of \p uranges.
 * \param[in] uranges A pack of ranges.
 * \details
 *
 * Zip multiple ranges into a range of tuples.
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
 *
 * In this library there are two objects corresponding to std::views::zip:
 *   * radr::zip: a factory object that is very similar to std::views::zip.
 *   * radr::zip_with: a range adaptor that allows the pipe call pattern, but does not allow container rvalues in \p other_ranges.
 *
 * ### Concepts
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * LValues of containers need to be wrapped in `std::ref()` or `std::cref()`.
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
 * ### Notable differences to std::views::zip
 *
 * * lvalues of containers need to be std::ref-wrapped.
 * * At least one argument needs to be given.
 */

inline constexpr auto zip = []<typename... Ranges>(Ranges &&... ranges)
{
    if constexpr ((safe_indirect_mp_range<Ranges> && ...))
    {
        // return plain adaptor if all inputs are borrowed
        return detail::zip_with_borrow_impl<detail::zip_iterator_kind::adaptor>(
          detail::zip_deref{},
          borrow(std::forward<Ranges>(ranges))...);
    }
    else if constexpr (((mp_range<Ranges> || ref_wrapped_mp_range<Ranges>)&&...))
    {
        static_assert((!container_lvalue<Ranges> && ...),
                      "Do not pass lvalues of containers to radr::zip. "
                      "To store copies, pass copies; to store references, wrap inputs in std::ref().");
        return zip_rng{RADR_FWD(ranges)...};
    }
    else
    {
        static_assert(false, "To zip over single-pass ranges, use radr::zip_sp.");
    }
};

/*!\brief Zips single-pass ranges.
 * \tparam URanges Type of \p uranges.
 * \param[in] uranges A pack of ranges.
 * \details
 *
 * Zips multiple ranges (single-pass and/or multi-pass) into a combined single-pass range.
 *
 * ### Concepts
 *
 * Requirements for every argument:
 *   * Must model radr::fwdable_range.
 *   * Must not model radr::container_lvalue.
 */
inline constexpr auto zip_sp = []<typename... URanges_>(URanges_ &&... uranges_)
{
    static_assert((fwdable_range<URanges_> && ...), "Arguments to radr::zip_sp must model radr::fwdable_range.");
    static_assert(((!container_lvalue<URanges_>)&&...), RADR_ASSERTSTRING_RVALUE);

    auto impl = []<typename... URanges>(URanges &&... uranges)
    {
        static constexpr bool rval_workaround =
          ((!std::copy_constructible<std::ranges::range_reference_t<URanges>>) || ...);

        using val_t = std::tuple<std::ranges::range_value_t<URanges>...>;
        using ref_t = std::conditional_t<rval_workaround,
                                         std::tuple<std::ranges::range_reference_t<URanges>...> &&,
                                         std::tuple<std::ranges::range_reference_t<URanges>...>>;

        return [](auto... uranges_) -> radr::generator<ref_t, val_t>
        {
            std::tuple<iterator_t<URanges>...>       its{radr::begin(uranges_)...};
            std::tuple<sentinel_t<URanges>...> const ends{radr::end(uranges_)...};

            auto at_end = [&]<size_t... I>(std::index_sequence<I...>)
            {
                return ((std::get<I>(its) == std::get<I>(ends)) || ...);
            };

            while (!at_end(std::make_index_sequence<sizeof...(uranges)>{}))
            {
                co_yield detail::tuple_transform([](auto & it) -> decltype(auto) { return *it; }, its);
                detail::tuple_for_each([](auto & it) { ++it; }, its);
            }
        }(std::move(uranges)...);
    };

    return impl(RADR_FWD(uranges_)...);
};

} // namespace cpo
} // namespace radr

#endif
