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

#include "../version.hpp"
#include "radr/class/zip_rng.hpp"

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include <ranges>

#    include "../concepts.hpp"
#    include "../detail/detail.hpp"
#    include "../detail/pipe.hpp"
#    include "../factory/iota.hpp"
#    include "../factory/zip.hpp"

namespace radr::detail
{

inline constexpr auto enumerate_borrow = []<std::ranges::borrowed_range URange>(URange && urange)
    requires std::ranges::forward_range<URange>
{
    using diff_t = std::ranges::range_difference_t<URange>;
    if constexpr (std::ranges::sized_range<URange>)
    {
        auto io = radr::iota(diff_t{0}, static_cast<diff_t>(std::ranges::size(urange)));

        // this case is special because zip_with usually looses common-ness
        // but now we know statically that both ranges' end is in sync (by definition)
        if constexpr (radr::common_range<URange>)
        {
            auto beg  = make_zip_it<zip_iterator_kind::enumerate>(radr::begin(io), radr::begin(urange));
            auto sen  = make_zip_it<zip_iterator_kind::enumerate>(radr::end(io), radr::end(urange));
            auto cbeg = make_zip_it<zip_iterator_kind::enumerate>(radr::cbegin(io), radr::cbegin(urange));
            auto csen = make_zip_it<zip_iterator_kind::enumerate>(radr::cend(io), radr::cend(urange));

            return borrowing_rad{beg, sen, cbeg, csen, std::ranges::size(urange)};
        }
        else
        {
            return zip_with_borrow_impl<zip_iterator_kind::enumerate>(std::move(io), borrow(urange));
        }
    }
    else
    {
        return zip_with_borrow_impl<zip_iterator_kind::enumerate>(radr::iota(diff_t{0}), borrow(urange));
    }
};

inline constexpr auto enumerate_coro = []<std::ranges::input_range URange>(URange && urange)
{
    static_assert(!container_lvalue<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    using diff_t = std::ranges::range_difference_t<URange>;
    return radr::zip_sp(radr::iota_sp(diff_t{0}), std::move(urange));
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Enumerate a range, yielding (index, value) pairs.
 * \param urange The underlying range.
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
 *   * radr::common_range (but only if also sized)
 *   * radr::constant_range
 *   * radr::mutable_range (although the index element is always read-only)
 *
 * ### Single-pass adaptor
 *
 * Requirements on \p urange :
 *   * std::ranges::input_range
 *
 * Returns a radr::generator of `std::tuple<range_difference_t, std::ranges::ranges_reference_t<URange>>`.
 */
inline constexpr auto enumerate = detail::pipe_without_args_fn{detail::enumerate_coro, detail::enumerate_borrow};
} // namespace cpo
} // namespace radr

#endif
