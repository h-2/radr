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

#include "../concepts.hpp"
#include "../custom/subborrow.hpp"
#include "../detail/detail.hpp"
#include "../generator.hpp"
#include "take.hpp"

namespace radr::detail
{
// clang-format off
inline constexpr auto take_exactly_borrow = detail::overloaded{
  []<const_borrowable_range URange>(URange && urange, size_t const n)
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

  },
  []<const_borrowable_range URange>(URange && urange, size_t const n)
      requires(std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
  {
      return subborrow(std::forward<URange>(urange), 0ull, n);
  }};
// clang-format on

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto take_exactly = detail::pipe_with_args_fn{detail::take_coro, detail::take_exactly_borrow};
} // namespace cpo
} // namespace radr
