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

//!\brief An alias that defines empty_rng in terms of an statically empty repeat_rng.
template <typename Value>
using empty_rng = repeat_rng<Value, constant_t<0>, repeat_rng_storage::indirect>;

/*!\brief A variable template that is a statically empty range.
 * \tparam Value The element type of the range; must be an object type (`const` allowed, reference not).
 *
 * \details
 *
 * This is technically not a factory (do not invoke it with `()` or `{}`), simply provide the Value
 * template parameter, e.g. `radr::empty<int>` to obtain the range.
 *
 * The actual type is radr::empty_rng which is in turn an alias for radr::repeat_rng.
 */
template <typename Value>
inline constinit empty_rng<Value> empty{};

} // namespace radr
