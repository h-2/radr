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

#include "radr/factory/iota.hpp"

namespace radr::detail
{

struct indices_fn
{
    template <std::integral Bound>
    constexpr auto operator()(Bound bound) const
    {
        return iota(static_cast<Bound>(0), bound);
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief A shortcut for `radr::iota(0, bound)`.
 * \tparam Bound The type of \p bound; required to model std::integral.
 * \param[in] bound The bound.
 */
inline constexpr detail::indices_fn indices{};
} // namespace cpo
} // namespace radr
