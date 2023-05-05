// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 The LLVM Project
// Copyright (c) 2023 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <ranges>

namespace radr
{

enum class range_bounds_kind : bool
{
    unsized,
    sized
};

template <std::input_or_output_iterator Iter,
          std::sentinel_for<Iter>       Sent,
          typename CIter,
          typename CSent,
          range_bounds_kind Kind>
    requires(Kind == range_bounds_kind::sized || !std::sized_sentinel_for<Sent, Iter>)
class range_bounds;

} // namespace radr
