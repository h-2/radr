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

#include "_pipe.hpp"
#include "concepts.hpp"
#include "detail.hpp"
#include "generator.hpp"

namespace radr
{

inline constexpr auto take_borrow = []<std::ranges::borrowed_range URange>(URange && urange, size_t const n)
    requires(std::ranges::forward_range<URange>)
{
    static constexpr range_bounds_kind kind = range_bounds_kind::sized;
    // std::ranges::sized_range<Range> ? range_bounds_kind::sized : range_bounds_kind::unsized;

    static_assert(std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>,
                  "This is not yet implemented for ranges that are forward but not RA+sized.");

    // if constexpr (std::ranges::random_access_range<Range> && std::ranges::sized_range<Range>)
    // {
    using RangeBounds = range_bounds<std::ranges::iterator_t<URange>,
                                     std::ranges::iterator_t<URange>,
                                     detail::const_it_or_nullptr_t<URange>,
                                     detail::const_it_or_nullptr_t<URange>,
                                     kind>;

    return RangeBounds{std::ranges::begin(urange),
                       std::ranges::begin(urange) + std::min<size_t>(n, std::ranges::size(urange)),
                       std::min<size_t>(n, std::ranges::size(urange))};
    // }
};

inline constexpr auto take_coro = []<adaptable_range URange>(URange && urange, size_t const n)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);

    // we need to create inner functor so that it can take by value
    return
      [](auto         urange_,
         size_t const n) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        size_t i = 0;
        for (auto it = std::ranges::begin(urange_); it != std::ranges::end(urange_) && i < n; ++it, ++i)
            co_yield *it;
    }(std::move(urange), n);
};

} // namespace radr

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto take = detail::pipe_with_args_fn{take_coro, take_borrow};
} // namespace cpo
} // namespace radr::pipe
