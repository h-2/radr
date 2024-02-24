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

#include <ranges>

#include "../concepts.hpp"
#include "../custom/subborrow.hpp"
#include "../detail/detail.hpp"
#include "../generator.hpp"

namespace radr::detail
{

inline constexpr auto make_single_pass_coro =
  detail::overloaded{[]<movable_range URange>(URange && urange)
                         requires(std::ranges::forward_range<URange>)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);

    return
      [](auto urange_) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        co_yield elements_of(urange_);
    }(std::move(urange));
},
                     []<movable_range URange>(URange && urange) -> decltype(auto) // forward single-pass ranges as-is
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);

    return std::move(urange);
}};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto make_single_pass = detail::range_adaptor_closure_t{
  detail::overloaded{detail::make_single_pass_coro,
                     []<std::ranges::input_range URange>(std::reference_wrapper<URange> const & range)
                     {
                     return detail::make_single_pass_coro(borrow(static_cast<URange &>(range)));
                     }}
};

}

} // namespace radr
