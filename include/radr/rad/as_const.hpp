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

#include <ranges>

#include "../concepts.hpp"
#include "../custom/subborrow.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"

namespace radr::detail
{

inline constexpr auto as_const_borrow = []<borrowed_mp_range URange>(URange && urange)
{
    if constexpr (std::ranges::sized_range<URange>)
    {
        return radr::subborrow(urange, radr::cbegin(urange), radr::cend(urange), std::ranges::size(urange));
    }
    else
    {
        return radr::subborrow(urange, radr::cbegin(urange), radr::cend(urange));
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Turns a mutable range into a constant range.
 * \param urange The underlying range.
 *
 * If the std::ranges::range_reference_t of \p urange is `T &`, it will become `T const &`. Otherwise, no change
 * happens.
 *
 * ### Multi-pass adaptor
 *
 * * Requirements on \p urange : radr::mp_range
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::contiguous_range
 *   * radr::common_range
 *   * std::ranges::borrowed_range
 *
 * It does not preserve:
 *   * radr::mutable_range (obviously)
 *
 * ### Single-pass adaptor
 *
 * Is ill-formed on single-pass ranges.
 *
 */
inline constexpr auto as_const = detail::pipe_without_args_fn<void, decltype(detail::as_const_borrow)>{};
} // namespace cpo
} // namespace radr
