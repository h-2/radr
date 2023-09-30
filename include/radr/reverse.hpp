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

#include "concepts.hpp"
#include "detail/detail.hpp"
#include "detail/pipe.hpp"
#include "generator.hpp"
#include "subborrow.hpp"

namespace radr
{
// clang-format off
inline constexpr auto reverse_borrow = []<const_borrowable_range URange>(URange && urange)
    requires std::ranges::bidirectional_range<URange>
  {
      
      auto get_rbeg = [] (auto && rng)
      {
          if constexpr (requires { std::ranges::rbegin(rng); })
              return std::ranges::rbegin(rng);
          else
              return std::make_reverse_iterator(std::ranges::next(std::ranges::begin(rng), std::ranges::end(rng)));
      };
      
      auto get_rend = [] (auto && rng)
      {
          if constexpr (requires { std::ranges::rend(rng); })
              return std::ranges::rend(rng);
          else
              return std::make_reverse_iterator(std::ranges::begin(rng));
      };
      
      static_assert(std::same_as<decltype(get_rbeg(urange)), decltype(get_rend(urange))>);
      
      using It      = decltype(get_rbeg(urange));
      using ConstIt = decltype(get_rbeg(std::as_const(urange)));
    
      static_assert(std::convertible_to<It, ConstIt>);
      
      It beg = get_rbeg(urange);        
      It e   = get_rend(urange);
        
      if constexpr (std::ranges::sized_range<URange>)
      {
          using BorrowingRad = borrowing_rad<It, It, ConstIt, ConstIt, borrowing_rad_kind::sized>;
          return BorrowingRad{std::move(beg), std::move(e), std::ranges::size(urange)};
      }
      else
      {
          using BorrowingRad = borrowing_rad<It, It, ConstIt, ConstIt, borrowing_rad_kind::unsized>;
          return BorrowingRad{std::move(beg), std::move(e)};
      }
  };
// clang-format on

} // namespace radr

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto reverse = detail::pipe_without_args_fn<void, decltype(reverse_borrow)>{};
} // namespace cpo
} // namespace radr::pipe
