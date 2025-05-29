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

#include <ranges>

#include "../concepts.hpp"
#include "../custom/subborrow.hpp"
#include "../detail/detail.hpp"
#include "../generator.hpp"

namespace radr::detail
{

inline constexpr auto to_single_pass_coro = detail::overloaded{
  []<std::ranges::forward_range URange>(URange && urange)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    return
      [](auto urange_) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        co_yield elements_of(urange_);
    }(std::move(urange));
},
  []<std::ranges::input_range URange>(URange && urange) -> decltype(auto) // forward single-pass ranges as-is
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    return std::move(urange);
}};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto to_single_pass = detail::range_adaptor_closure_t{
  detail::overloaded{detail::to_single_pass_coro,
                     []<std::ranges::input_range URange>(std::reference_wrapper<URange> const & range)
                     {
                     return detail::to_single_pass_coro(borrow(static_cast<URange &>(range)));
                     }}
};

[[deprecated(
  "Use radr::to_single_pass instead of std::views::to_input, but see the docs regarding semantic "
  "differences.")]] inline constexpr auto to_input = to_single_pass;

} // namespace cpo

} // namespace radr
