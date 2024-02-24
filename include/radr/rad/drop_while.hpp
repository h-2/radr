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
#include "../detail/pipe.hpp"
#include "../generator.hpp"

namespace radr::detail
{
// clang-format off
inline constexpr auto drop_while_borrow =
  []<std::ranges::borrowed_range URange, std::indirect_unary_predicate<std::ranges::iterator_t<URange>> Fn>
    (URange && urange, Fn fn)
  {
      auto it  = radr::begin(urange);
      auto e   = radr::end(urange);

      size_t count = 0ull;
      for (; it != e && fn(*it); ++it, ++count)
      {}

      if constexpr (std::ranges::sized_range<URange>)
      {
          assert(count <= std::ranges::size(urange));
          return subborrow(std::forward<URange>(urange), it, e, std::ranges::size(urange) - count);
      }
      else
      {
          return subborrow(std::forward<URange>(urange), it, e);
      }
  };
// clang-format on

inline constexpr auto drop_while_coro =
  []<movable_range URange, std::indirect_unary_predicate<std::ranges::iterator_t<URange>> Fn>(URange && urange, Fn fn)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);

    // we need to create inner functor so that it can drop by value
    return [](auto urange_,
              Fn   fn_) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        auto it = radr::begin(urange_);
        auto e  = radr::end(urange_);

        for (; it != e && fn_(*it); ++it)
        {
        }

        for (; it != e; ++it)
            co_yield *it;
    }(std::move(urange), fn);
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto drop_while = detail::pipe_with_args_fn{detail::drop_while_coro, detail::drop_while_borrow};
} // namespace cpo
} // namespace radr
