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
#include "../custom/subborrow.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../generator.hpp"

namespace radr::detail
{
inline constexpr auto drop_borrow =
  detail::overloaded{[]<std::ranges::borrowed_range URange>(URange && urange, size_t const n)
                         requires std::ranges::forward_range<URange>
{
    auto it  = radr::begin(urange);
    auto end = radr::end(urange);

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
}};

inline constexpr auto drop_coro = []<std::ranges::input_range URange>(URange && urange, size_t const n)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    // we need to create inner functor so that it can drop by value
    return
      [](auto         urange_,
         size_t const n) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        auto it = radr::begin(urange_);
        auto e  = radr::end(urange_);

        std::ranges::advance(it, n, e);
        for (; it != e; ++it)
            co_yield *it;
    }(std::move(urange), n);
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Drop up to `n` elements from the prefix of a range.
 * \param urange The underlying range.
 * \param[in] n The number of elements to drop (at most).
 *
 * ### Multi-pass adaptor
 *
 * * Requirements on \p urange : radr::mp_range
 *
 * Construction time:
 *   * O(1) if \p urange models std::ranges::random_access_range and std::ranges::size_range.
 *   * O(n) otherwise
 *
 * Unlike std::views::drop, this adaptor searches for begin on construction. This preserves more concepts (see
 * below).
 *
 * This adaptor always invokes the radr::subborrow customisation point.
 *
 * Unless customised otherwise, this adaptor preserves:
 *   * categories up to std::ranges::contiguous_range
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::common_range
 *   * radr::constant_range
 *   * radr::mutable_range
 *
 * Unless customised otherwise, this adaptor is transparent, i.e. radr::iterator_t and radr::sentinel_t are preserved.
 *
 * ### Single-pass adaptor
 *
 * * Requirements on \p urange : std::ranges::input_range
 *
 */

inline constexpr auto drop = detail::pipe_with_args_fn{detail::drop_coro, detail::drop_borrow};
} // namespace cpo
} // namespace radr
