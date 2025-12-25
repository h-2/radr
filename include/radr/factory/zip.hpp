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

#include <algorithm>
#include <concepts>
#include <functional>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

#include "../concepts.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../detail/semiregular_box.hpp"
#include "../generator.hpp"
#include "radr/rad/zip_with.hpp"
#include "radr/rad_util/rad_interface.hpp"

#if RADR_COMMON_TUPLE

namespace radr
{

template <typename ...Ranges>
class zip_rng : public rad_interface<zip_rng<Ranges...>>
{
private:
    static_assert((detail::owned_range_constraints<Ranges> && ...),
                  "All arguments to radr::zip_rng must be copyable objects (no ref, no const).");

    std::tuple<Ranges...>  ranges;

public:

    zip_rng(Ranges ... rngs) : ranges{std::move(rngs)...}
    {}

    auto begin()
    {
        return detail::zip_iterator<Ranges...>


};





inline namespace cpo
{
/*!\brief Zips ranges with the given one.
 * \tparam URange Type of \p urange.
 * \tparam OtherRanges Types of \p other_ranges.
 * \param[in] urange The underlying range.
 * \param[in] other_ranges A pack of the other ranges; each wrapped in std::ref or std::cref.
 * \details
 *
 * |                                              |  radr::zip     |  radr::zip_with      |
 * |----------------------------------------------|----------------|----------------------|
 * | minimum arguments                            |   1            |                  1   |
 * | pipe into                                    |    no          |               yes    |
 * | lvalues of containers allowed (first range)  | std::ref-wrapped |    std::ref-wrapped |
 * | lvalues of containers allowed (other ranges) | std::ref-wrapped |    std::ref-wrapped |
 * | rvalues of containers allowed (first range)  | yes             |      yes            |
 * | rvalues of containers allowed (other ranges) | yes             |      no             |
 *
 * Use `radr::zip` when you need to capture more than one container by rvalue.
 *
 * Use `radr::zip_with` if you need pipe-support.
 *
 * ## Multi-pass adaptor
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * Each type in `OtherRanges` needs to be std::reference_wrapper around a `radr::mp_range`.
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
 */

inline constexpr auto zip = []<typename ... Ranges>(Ranges ... ranges)
{
    static_assert(sizeof...(ranges) > 0, "Must provide at least one argument to radr::zip.");
    static_assert(((mp_range<Ranges> || ref_wrapped_range<Ranges>) && ...),
                  "All arguments to radr::zip need to be ranges.");
    static_assert(((!std::is_lvalue_reference_v<Ranges> || explicitly_borrowed_range<Ranges>) && ...),
                  RADR_ASSERTSTRING_RVALUE);

    if constexpr ((explicitly_borrowed_range<Ranges> && ...))
    {
        return zip_with(std::forward<Ranges>(ranges)...);
    }
    else
    {
        auto fwd_unwrap = []<typename Rng>(Rng && rng) -> decltype(auto)
        {
            if constexpr (explicitly_borrowed_range<Rng>)
                return borrow(rng);
            else
                std::forward<Rng>(rng);
        };
        return zip_rng{fwd_unwrap(std::forward<Ranges>(ranges)...)};
    }
};
} // namespace cpo
} // namespace radr

#else

#error "C++23 required for radr::zip_with"
#endif
