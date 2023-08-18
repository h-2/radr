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

inline constexpr auto drop_borrow = detail::overloaded{
  []<std::ranges::borrowed_range URange>(URange && urange, size_t const n) requires(std::ranges::forward_range<URange>){
    static constexpr borrowing_rad_kind kind =
      std::ranges::sized_range<URange> ? borrowing_rad_kind::sized : borrowing_rad_kind::unsized;

using BorrowingRad = borrowing_rad<std::ranges::iterator_t<URange>,
                                   std::ranges::sentinel_t<URange>,
                                   detail::const_it_or_nullptr_t<URange>,
                                   detail::const_sen_or_nullptr_t<URange>,
                                   kind>;

auto it  = std::ranges::begin(urange);
auto end = std::ranges::end(urange);

std::ranges::advance(it, n, end);

if constexpr (std::ranges::sized_range<URange>)
{
    return BorrowingRad{it, end, n > std::ranges::size(urange) ? 0ull : std::ranges::size(urange) - n};
}
else
{
    return BorrowingRad{it, end};
}
} // namespace radr
, []<subborrowable_range URange>(URange && urange, size_t const n)
{ return subborrow(std::forward<URange>(urange), n, std::ranges::size(urange)); }
}
;

inline constexpr auto drop_coro = []<movable_range URange>(URange && urange, size_t const n)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);

    // we need to create inner functor so that it can drop by value
    return
      [](auto         urange_,
         size_t const n) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        auto it = std::ranges::begin(urange_);
        std::ranges::advance(it, n, std::ranges::end(urange_));
        for (; it != std::ranges::end(urange_); ++it)
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
