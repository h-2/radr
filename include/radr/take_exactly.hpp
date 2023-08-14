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

#include <functional>
#include <ranges>

#include "cached_bounds.hpp"
#include "concepts.hpp"
#include "detail.hpp"
#include "generator.hpp"
#include "rad_interface.hpp"
#include "subborrow.hpp"
#include "take.hpp"

namespace radr
{
// clang-format off
inline constexpr auto take_exactly_borrow = detail::overloaded{
  []<std::ranges::borrowed_range URange>(URange && urange, size_t const n)
      requires(std::ranges::forward_range<URange>)
  {
      if constexpr (const_iterable_range<URange>)
      {
          using URangeNoRef = std::remove_reference_t<URange>;
          using BorrowingRad = borrowing_rad<std::counted_iterator<std::ranges::iterator_t<URangeNoRef>>,
                                             std::default_sentinel_t,
                                             std::counted_iterator<std::ranges::iterator_t<URangeNoRef const>>,
                                             std::default_sentinel_t,
                                             borrowing_rad_kind::sized>;

          return BorrowingRad{std::counted_iterator<std::ranges::iterator_t<URange>>(std::ranges::begin(urange), n),
                              std::default_sentinel,
                              n};
      }
      else
      {
          using BorrowingRad = borrowing_rad<std::counted_iterator<std::ranges::iterator_t<URange>>,
                                             std::default_sentinel_t,
                                             std::nullptr_t,
                                             std::nullptr_t,
                                             borrowing_rad_kind::sized>;

          return BorrowingRad{std::counted_iterator<std::ranges::iterator_t<URange>>(std::ranges::begin(urange), n),
                              std::default_sentinel,
                              n};
      }
  },
  []<subborrowable_range URange>(URange && urange, size_t const n)
  {
      return subborrow(std::forward<URange>(urange), 0, n);
  }};
// clang-format on

} // namespace radr

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto take_exactly = detail::pipe_with_args_fn{take_coro, take_exactly_borrow};
} // namespace cpo
} // namespace radr::pipe
