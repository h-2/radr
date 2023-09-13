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
inline constexpr auto drop_borrow = detail::overloaded{
  []<std::ranges::borrowed_range URange>(URange && urange, size_t const n)
    requires std::ranges::forward_range<URange>
  {
      auto it  = std::ranges::begin(urange);
      auto end = std::ranges::end(urange);

      std::ranges::advance(it, n, end);

      if constexpr (std::ranges::sized_range<URange>)
      {
          return subborrow(std::forward<URange>(urange),
                           it,
                           end,
                           n > std::ranges::size(urange) ? 0ull : std::ranges::size(urange) - n);
      }
      else
      {
          return subborrow(std::forward<URange>(urange), it, end);
      }
  }
};
// clang-format on

inline constexpr auto drop_coro = []<movable_range URange>(URange && urange, size_t const n)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);

    // we need to create inner functor so that it can drop by value
    return
      [](auto         urange_,
         size_t const n) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        auto it = std::ranges::begin(urange_);
        auto e  = std::ranges::end(urange_);

        std::ranges::advance(it, n, e);
        for (; it != e; ++it)
            co_yield *it;
    }(std::move(urange), n);
};

} // namespace radr

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto drop = detail::pipe_with_args_fn{drop_coro, drop_borrow};
} // namespace cpo
} // namespace radr::pipe
