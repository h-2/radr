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

#include "radr/rad/adjacent_transform.hpp"

namespace radr
{

inline namespace cpo
{
/*!\brief Slides a window of size 2 over the range and applies an invocable to every window.
 * \param[in] urange The underlying range.
 * \param[in] fn The invocable to apply; it is called with 2 arguments, not with a tuple.
 *
 * This is the same as radr::adjacent_transform<2> and all respective documentation applies.
 */
inline constexpr auto pairwise_transform = adjacent_transform<2>;
} // namespace cpo
} // namespace radr
