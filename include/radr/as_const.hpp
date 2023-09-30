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
#include "subborrow.hpp"

namespace radr
{

inline constexpr auto as_const_borrow = []<const_borrowable_range URange>(URange && urange)
  {
      if constexpr (std::same_as<std::ranges::range_reference_t<URange>, decltype(*std::ranges::cbegin(urange))>)
      {
          return borrow(std::forward<URange>(urange));
      }
      else
      {
        auto b = std::ranges::cbegin(urange);
        auto e = std::ranges::cend(urange);
        
        using It   = decltype(b);
        using Sent = decltype(e);
            
        if constexpr (std::ranges::sized_range<URange>)
        {
            using BorrowingRad = borrowing_rad<It, Sent, It, Sent, borrowing_rad_kind::sized>;
            return BorrowingRad{std::move(b), std::move(e), std::ranges::size(urange)};
        }
        else
        {
            using BorrowingRad = borrowing_rad<It, Sent, It, Sent, borrowing_rad_kind::unsized>;
            return BorrowingRad{std::move(b), std::move(e)};
        }
      }
  };
 
} // namespace radr

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto as_const = detail::pipe_without_args_fn<void, decltype(as_const_borrow)>{};
} // namespace cpo
} // namespace radr::pipe
