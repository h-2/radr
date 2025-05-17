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

#include <functional>
#include <ranges>

#include "../concepts.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../rad_util/borrowing_rad.hpp"

namespace radr::detail
{
inline constexpr auto reverse_borrow = []<const_borrowable_range URange>(URange && urange)
    requires std::ranges::bidirectional_range<URange>
{
    //TODO we need proper radr::rbegin, radr::rend, radr::crbegin and radr::crend
    auto get_rbeg = [](auto && rng)
    {
        if constexpr (requires { std::ranges::rbegin(rng); })
            return std::ranges::rbegin(rng);
        else
            return std::make_reverse_iterator(std::ranges::next(radr::begin(rng), radr::end(rng)));
    };

    auto get_rend = [](auto && rng)
    {
        if constexpr (requires { std::ranges::rend(rng); })
            return std::ranges::rend(rng);
        else
            return std::make_reverse_iterator(radr::begin(rng));
    };

    static_assert(std::same_as<decltype(get_rbeg(urange)), decltype(get_rend(urange))>);

    using It      = decltype(get_rbeg(urange));
    using ConstIt = decltype(get_rbeg(std::as_const(urange)));

    static_assert(std::convertible_to<It, ConstIt>);

    It beg = get_rbeg(urange);
    It e   = get_rend(urange);

    static constexpr auto kind =
      std::ranges::sized_range<URange> ? borrowing_rad_kind::sized : borrowing_rad_kind::unsized;
    using BorrowingRad = borrowing_rad<It, It, ConstIt, ConstIt, kind>;
    return BorrowingRad{std::move(beg), std::move(e), size_or_not(urange)};
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto reverse = detail::pipe_without_args_fn<void, decltype(detail::reverse_borrow)>{};
} // namespace cpo
} // namespace radr
