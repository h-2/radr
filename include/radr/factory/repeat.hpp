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

#include "radr/class/repeat_rng.hpp"

namespace radr::detail
{

struct repeat_t
{
    template <typename TVal>
    constexpr auto operator()(TVal && val) const
    {
        return repeat_rng{std::forward<TVal>(val)};
    }

    template <typename TVal, typename Bound>
    constexpr auto operator()(TVal && val, Bound && bound) const
        requires requires {
            repeat_rng{std::forward<TVal>(val), std::forward<Bound>(bound)};
        }
    {
        return repeat_rng{std::forward<TVal>(val), std::forward<Bound>(bound)};
    }
};

} // namespace radr::detail

namespace radr
{
/*!\brief A range factory that produces a range which repeats a specific element 0-N (possibly infinite) times.
 * \param value The value.
 * \param bound The bound; optional; defaults to std::unreachable_sentinel.
 *
 * \details
 *
 * This factory returns an object of type radr::repeat_rng. See the respective documentation for details.
 *
 * In particular, note that for the value parameter:
 *
 *   * By default, the value passed will be stored in the range.
 *   * Small values will also be copied into the iterator.
 *   * Values wrapped in `std::ref()` will be referenced (not stored).
 *
 * For the bound parameter:
 *   * An integral value will lead to a dynamic / run-time bound of that value.
 *   * `radr::constant<X>` will lead to a static / compile-time bound of X.
 *   * std::unreachable_sentinel will lead to an unbounded / infinite range. This is the default.
 */
inline constexpr detail::repeat_t repeat{};

} // namespace radr
