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

#include "../custom/subborrow.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../generator.hpp"

namespace radr::detail
{

inline constexpr auto drop_while_borrow =
  []<std::ranges::borrowed_range URange, std::indirect_unary_predicate<std::ranges::iterator_t<URange>> Fn>(
    URange && urange,
    Fn        fn)
{
    auto it = radr::begin(urange);
    auto e  = radr::end(urange);

    size_t count = 0ull;
    for (; it != e && fn(*it); ++it, ++count)
    {
    }

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

inline constexpr auto drop_while_coro =
  []<std::ranges::input_range URange, std::indirect_unary_predicate<std::ranges::iterator_t<URange>> Fn>(
    URange && urange,
    Fn        fn)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

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

/*!\brief Drop elements from the prefix of a range while the filter predicate holds true.
 * \param urange The underlying range.
 * \param[in] predicate The predicate to filter by.
 *
 * ### Multi-pass adaptor
 *
 * * Requirements on \p urange : radr::mp_range
 * * Requirements on \p predicate : std::move_constructible, std::is_object_v, std::indirect_unary_invocable on \p urange 's iterator_t
 *
 * Construction of the adaptor is in O(n), because the first matching element is searched and cached.
 * Unlike radr::filter and radr::take_while and unlike std::views::drop_while, this adaptor evaluates its predicate
 * completely on construction. This allows for state changes within \p predicate and preserves more concepts (see
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
 * * Requirements on \p predicate : std::move_constructible, std::is_object_v, std::indirect_unary_invocable on \p urange 's iterator_t
 *
 */
inline constexpr auto drop_while = detail::pipe_with_args_fn{detail::drop_while_coro, detail::drop_while_borrow};
} // namespace cpo
} // namespace radr
