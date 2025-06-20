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

#include <iterator>

#include "../rad/take.hpp"

namespace radr
{

inline namespace cpo
{

/*!\brief Create a range from an iterator and a count.
 * \tparam It The type of \p it; required to model std::forward_iterator.
 * \param[in] it The iterator.
 * \param[in] n  The count.
 * \details
 *
 * In contrast to std::views::counted, this factory always returns a multi-pass range and thus requires
 * `std::forward_iterator<It>` and not just `std::input_or_output_iterator<It>`.
 *
 * There is radr::counted_sp which is always a single-pass range and does not have this requirement.
 *
 * Unlike std::views::counted, we consider radr::counted a range factory and not a range adaptor, because
 * you cannot pipe a range into it.
 *
 * ### Safety
 *
 * Note that this function (like std::views::counted) performs no bounds-checking.
 */
inline constexpr auto counted = []<std::forward_iterator It>(It it, size_t const n)
{
    /* this is a bit of a hack; the sentinels are irrelevant and also the first size parameter
     * but take_borrow will dispatch to either subborrow (for ra-iter) or else
     * (std::counted_iterator, default_sentinel) which is what we want.
     */
    return detail::take_borrow(
      borrowing_rad{it, std::unreachable_sentinel, make_const_iterator(it), std::unreachable_sentinel, n},
      n);
};

/*!\brief Single-pass version of radr::counted.
 * \tparam It The type of \p it; required to model std::input_or_output_iterator.
 * \param[in,out] it The iterator.
 * \param[in] n  The count.
 * \details
 *
 * This factory always returns a radr::generator, a move-only, single-pass range.
 *
 * The requirements on the iterator type are weaker than for radr::counted.
 *
 * ### Safety
 *
 * Note that this function (like std::views::counted) performs no bounds-checking.
 */
inline constexpr auto counted_sp = []<typename It>(It && it, size_t const n)
{
    static_assert(std::input_or_output_iterator<std::remove_cvref_t<It>>,
                  "You must pass an iterator as first argument to radr::counted_sp().");

    // Intentionally, no forwarding reference in this signature
    return [](It it, size_t const n) -> generator<std::iter_reference_t<It>, std::iter_value_t<It>>
    {
        uint64_t i = 0;
        while (i < n)
        {
            co_yield *it;
            ++i;
            ++it;
        }
    }(std::forward<It>(it), n);
};

} // namespace cpo
} // namespace radr
