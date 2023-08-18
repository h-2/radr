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
#include <utility>

#include "../borrow.hpp"
#include "../owning_rad.hpp"
#include "detail.hpp"

namespace radr::detail
{

template <typename... Args>
concept non_empty_args = (sizeof...(Args) > 0);

template <typename CoroFn, typename BorrowFn, bool closure = false>
struct pipe_with_args_fn
{
    constexpr pipe_with_args_fn() noexcept = default;
    constexpr pipe_with_args_fn(CoroFn, BorrowFn) noexcept {}

    template <movable_range Range, class... Args>
        requires non_empty_args<Args...>
    [[nodiscard]] constexpr auto operator()(Range && range, Args &&... args) const
      noexcept(noexcept(CoroFn{}(std::forward<Range>(range), std::forward<Args>(args)...)))
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_RVALUE_ASSERTION_STRING);
        return CoroFn{}(std::forward<Range>(range), std::forward<Args>(args)...);
    }

    template <movable_range Range, class... Args>
        requires (non_empty_args<Args...> && std::ranges::forward_range<Range>)
    [[nodiscard]] constexpr auto operator()(Range && range, Args &&... args) const noexcept(
      noexcept(owning_rad{std::forward<Range>(range), detail::bind_back(BorrowFn{}, std::forward<Args>(args)...)}))
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_RVALUE_ASSERTION_STRING);
        //TODO static_assert that range is copyable and const_iterable?
        return owning_rad{std::forward<Range>(range), detail::bind_back(BorrowFn{}, std::forward<Args>(args)...)};
    }

    template <movable_range Range, class... Args>
        requires(non_empty_args<Args...> && std::ranges::forward_range<Range> && std::ranges::borrowed_range<Range>)
    [[nodiscard]] constexpr auto operator()(Range && range, Args &&... args) const
      noexcept(noexcept(BorrowFn{}(std::forward<Range>(range), std::forward<Args>(args)...)))
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_RVALUE_ASSERTION_STRING);
        //TODO static_assert that we only borrow from forward ranges?
        return BorrowFn{}(std::forward<Range>(range), std::forward<Args>(args)...);
    }

    template <std::ranges::input_range Range, class... Args>
        requires non_empty_args<Args...>
    [[nodiscard]] constexpr auto operator()(std::reference_wrapper<Range> const & range, Args &&... args) const
      noexcept(noexcept(operator()(borrow(static_cast<Range &>(range)), std::forward<Args>(args)...)))
        -> decltype(operator()(borrow(static_cast<Range &>(range)), std::forward<Args>(args)...))
    {
        return operator()(borrow(static_cast<Range &>(range)), std::forward<Args>(args)...);
    }

    template <class... Args>
        requires(non_empty_args<Args...> && closure == false)
    [[nodiscard]] constexpr auto operator()(Args &&... args) const
      noexcept((std::is_nothrow_constructible_v<std::decay_t<Args>, Args> && ...))
    {
        static_assert((std::constructible_from<std::decay_t<Args>, Args> && ...));
        return range_adaptor_closure_t{
          detail::bind_back(pipe_with_args_fn<CoroFn, BorrowFn, true>{}, std::forward<Args>(args)...)};
    }
};

} // namespace radr::detail
