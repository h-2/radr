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

namespace radr
{
// clang-format off
inline constexpr auto as_rvalue_borrow = []<const_borrowable_range URange>(URange && urange)
  {
    if constexpr (std::same_as<std::ranges::range_rvalue_reference_t<URange>, std::ranges::range_reference_t<URange>>)
    {
        return borrow(std::forward<URange>(urange));
    }
    else
    {
#ifdef __cpp_lib_move_iterator_concept 
        auto get_mbeg = [] (auto && rng)
        {
            return std::move_iterator(std::ranges::begin(rng));
        };
        
        auto get_mend = [] (auto && rng)
        {
            if constexpr (std::ranges::common_range<decltype(rng)>)
                return std::move_iterator(std::ranges::end(rng));
            else
                return std::move_sentinel(std::ranges::end(rng));
        };
        
        using It        = decltype(get_mbeg(urange));
        using Sent      = decltype(get_mend(urange));
        using ConstIt   = decltype(get_mbeg(std::as_const(urange)));
        using ConstSent = decltype(get_mend(std::as_const(urange)));
        
        static_assert(std::convertible_to<It, ConstIt>);
        static_assert(std::convertible_to<Sent, ConstSent>);
        
        It b = get_mbeg(urange);        
        It e = get_mend(urange);
            
        if constexpr (std::ranges::sized_range<URange>)
        {
            using BorrowingRad = borrowing_rad<It, Sent, ConstIt, ConstSent, borrowing_rad_kind::sized>;
            return BorrowingRad{std::move(b), std::move(e), std::ranges::size(urange)};
        }
        else
        {
            using BorrowingRad = borrowing_rad<It, Sent, ConstIt, ConstSent, borrowing_rad_kind::unsized>;
            return BorrowingRad{std::move(b), std::move(e)};
        }
#else
    static_assert(!std::ranges::range<URange> /*always false*/,
                  "radr::as_rvalue on multi-pass ranges is only supported in C++23.");
#endif
    }
  };
// clang-format on

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

} // namespace radr

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto as_rvalue = detail::pipe_without_args_fn{as_rvalue_coro, as_rvalue_borrow};
} // namespace cpo
} // namespace radr::pipe
