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

#include <cstddef>

#include "radr/version.hpp"

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include "radr/rad/adjacent.hpp"

namespace radr
{

inline namespace cpo
{
/*!\brief Create a sliding tuple of size 2 over the range.
 * \param urange The underlying range.
 *
 * This is the same as radr::adjacent<2> and all respective documentation applies.
 */
inline constexpr auto pairwise = adjacent<2>;
} // namespace cpo
} // namespace radr

#endif
