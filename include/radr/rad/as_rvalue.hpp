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
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../generator.hpp"
#include "../rad_util/borrowing_rad.hpp"

namespace radr::detail
{

inline constexpr auto as_rvalue_borrow = []<const_borrowable_range URange>(URange && urange)
{
    if constexpr (std::same_as<std::ranges::range_rvalue_reference_t<URange>, std::ranges::range_reference_t<URange>>)
    {
        return borrow(std::forward<URange>(urange));
    }
    else
    {
#ifdef __cpp_lib_move_iterator_concept
        auto get_mbeg = [](auto b)
        {
            return std::move_iterator(std::move(b));
        };

        auto get_mend = []<typename B, typename E>(B, E e)
        {
            if constexpr (std::same_as<B, E>)
                return std::move_iterator(std::move(e));
            else
                return std::move_sentinel(std::move(e));
        };

        using It        = decltype(get_mbeg(radr::begin(urange)));
        using Sent      = decltype(get_mend(radr::begin(urange), radr::end(urange)));
        using ConstIt   = decltype(get_mbeg(radr::cbegin(urange)));
        using ConstSent = decltype(get_mend(radr::cbegin(urange), radr::cend(urange)));

        static_assert(std::convertible_to<It, ConstIt>);
        static_assert(std::convertible_to<Sent, ConstSent>);

        It b = get_mbeg(radr::begin(urange));
        It e = get_mend(radr::begin(urange), radr::end(urange));

        static constexpr auto kind =
          std::ranges::sized_range<URange> ? borrowing_rad_kind::sized : borrowing_rad_kind::unsized;
        using BorrowingRad = borrowing_rad<It, Sent, ConstIt, ConstSent, kind>;
        return BorrowingRad{std::move(b), std::move(e), size_or_not(urange)};
#else
        static_assert(!std::ranges::range<URange> /*always false*/,
                      "radr::as_rvalue on multi-pass ranges is only supported in C++23.");
#endif
    }
};

inline constexpr auto as_rvalue_coro = []<movable_range URange>(URange && urange)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);

    // we need to create inner functor so that it can take by value
    return [](auto urange_)
             -> radr::generator<std::ranges::range_rvalue_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        if constexpr (
          std::same_as<std::ranges::range_reference_t<URange>, std::ranges::range_rvalue_reference_t<URange>>)
        {
            co_yield elements_of(urange_);
        }
        else
        {
            for (auto && elem : urange_)
                co_yield std::move(elem);
        }
    }(std::move(urange));
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto as_rvalue = detail::pipe_without_args_fn{detail::as_rvalue_coro, detail::as_rvalue_borrow};
} // namespace cpo
} // namespace radr
