// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2024 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <functional>
#include <ranges>

#include "../concepts.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../rad_util/borrowing_rad.hpp"
#include "radr/custom/subborrow.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{
inline constexpr auto to_common_borrow = []<const_borrowable_range URange>(URange && urange)
{
    if constexpr (common_range<URange>)
    {
        return borrow(urange);
    }
    else
    {
        auto   b     = radr::begin(urange);
        auto   old_e = radr::end(urange);
        auto   new_e = b;
        size_t s     = 0;
        for (; new_e != old_e; ++new_e, ++s)
        {
        }

        return subborrow(urange, b, new_e, s);
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
 * ## Multi-pass ranges
 *
 * For ranges that model radr::common_range: returns an identity/NOOP adaptor.
 *
 * For ranges that do not model radr::common_range:
 *  * Returns a range adaptor that models radr::common_range.
 *  * The end is determined by searching linearly from the beginning.
 *  * The returned adaptor also caches the size and models std::ranges::sized_range.
 *
 * ## Single-pass ranges
 *
 * This adaptor cannot be created on single-pass ranges.
 *
 */

inline constexpr auto to_common = detail::pipe_without_args_fn<void, decltype(detail::to_common_borrow)>{};
} // namespace cpo
} // namespace radr
