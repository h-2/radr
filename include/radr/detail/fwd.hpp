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

namespace radr
{

enum class borrowing_rad_kind : bool
{
    unsized,
    sized
};

template <std::forward_iterator    Iter,
          std::sentinel_for<Iter>  Sent,
          std::forward_iterator    CIter,
          std::sentinel_for<CIter> CSent,
          borrowing_rad_kind       Kind>
    requires(Kind == borrowing_rad_kind::sized || !std::sized_sentinel_for<Sent, Iter>)
class borrowing_rad;

} // namespace radr
