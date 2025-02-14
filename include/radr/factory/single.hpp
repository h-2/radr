// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2025 Hannes Hauswedell
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include "radr/factory/repeat.hpp"

namespace radr
{

//!\brief Alias for single value range types.
template <typename TVal, repeat_rng_storage storage>
using single_rng = repeat_rng<TVal, constant_t<1>, storage>;

/*!\brief A range factory that produces a range of a single value.
 * \param value The value.
 *
 * \details
 *
 * This factory forwards to `radr::repeat(value, radr::constant<1>)` and thus returns
 * an object of type radr::repeat_rng. See the respective documentation for details.
 *
 * In particular:
 *
 *   * By default, the value passed will be stored in the range.
 *   * Small values will also be copied into the iterator.
 *   * Values wrapped in `std::ref()` will be referenced (not stored).
 */
inline constexpr auto single = []<class T>(T && val)
    requires requires { repeat(std::forward<T>(val), constant<1>); }
{
    return repeat(std::forward<T>(val), constant<1>);
};

} // namespace radr
