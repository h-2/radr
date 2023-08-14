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

#include "borrow.hpp"
#include "concepts.hpp"
#include "detail/detail.hpp"
#include "generator.hpp"

namespace radr
{

inline constexpr auto make_single_pass_coro = []<adaptable_range URange>(URange && urange)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);

    // we need to create inner functor so that it can take by value
    return
      [](auto urange_) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    { co_yield elements_of(urange_); }(std::move(urange));
};

} // namespace radr

namespace radr::pipe
{

inline constexpr auto make_single_pass = detail::range_adaptor_closure_t{
  detail::overloaded{make_single_pass_coro,
                     []<std::ranges::input_range URange>(std::reference_wrapper<URange> const & range)
                     { return make_single_pass_coro(borrow(static_cast<URange &>(range))); }}
};
} // namespace radr::pipe
