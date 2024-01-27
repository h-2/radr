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

#include "../custom/subborrow.hpp"
#include "../rad_util/owning_rad.hpp"
#include "detail.hpp"

namespace radr::detail
{

struct empty_t
{};

template <bool nonzero, typename... Args>
concept arg_count = (bool(sizeof...(Args)) == nonzero);

template <typename CoroFn, bool non_empty_args>
struct pipe_input_base
{
    //!\brief Forward into Coroutine-Functor.
    template <movable_range Range, class... Args>
        requires(arg_count<non_empty_args, Args...>)
    [[nodiscard]] constexpr auto operator()(Range && range, Args &&... args) const
      noexcept(noexcept(CoroFn{}(std::forward<Range>(range), std::forward<Args>(args)...)))
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_ASSERTSTRING_RVALUE);

        return CoroFn{}(std::forward<Range>(range), std::forward<Args>(args)...);
    }
};

template <bool non_empty_args>
struct pipe_input_base<void, non_empty_args>
{
    struct noop_
    {};
    void operator()(noop_) {}
};

template <typename BorrowFn, bool non_empty_args>
struct pipe_fwd_base
{
    //!\brief Forward into Owning Range Adaptor.
    template <movable_range Range, class... Args>
        requires(arg_count<non_empty_args, Args...> && std::ranges::forward_range<Range>)
    [[nodiscard]] constexpr auto operator()(Range && range, Args &&... args) const
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_ASSERTSTRING_RVALUE);
        static_assert(const_iterable_range<Range>, RADR_ASSERTSTRING_CONST_ITERABLE);
        static_assert(std::copyable<Range>, RADR_ASSERTSTRING_COPYABLE);

        return owning_rad{std::forward<Range>(range), detail::bind_back(BorrowFn{}, std::forward<Args>(args)...)};
    }

    //!\brief Forward into Borrowing Range Adaptor.
    template <movable_range Range, class... Args>
        requires(arg_count<non_empty_args, Args...> && std::ranges::forward_range<Range> &&
                 std::ranges::borrowed_range<Range> && std::semiregular<std::remove_cvref_t<Range>>)
    [[nodiscard]] constexpr auto operator()(Range && range, Args &&... args) const
      noexcept(noexcept(BorrowFn{}(std::forward<Range>(range), std::forward<Args>(args)...)))
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_ASSERTSTRING_RVALUE);
        static_assert(const_iterable_range<Range>, RADR_ASSERTSTRING_CONST_ITERABLE);

        return BorrowFn{}(std::forward<Range>(range), std::forward<Args>(args)...);
    }

    //!\brief Forward into Borrowing Range Adaptor. Borrowed Ranges that are not semiregular and made semiregular.
    template <movable_range Range, class... Args>
        requires(arg_count<non_empty_args, Args...> && std::ranges::forward_range<Range> &&
                 std::ranges::borrowed_range<Range>)
    [[nodiscard]] constexpr auto operator()(Range && range, Args &&... args) const
      noexcept(noexcept(BorrowFn{}(std::forward<Range>(range), std::forward<Args>(args)...)))
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_ASSERTSTRING_RVALUE);

        // we convert the range into a borrowing_rad which is always semiregular
        return operator()(radr::borrow(range), std::forward<Args>(args)...);
    }

    template <std::ranges::input_range Range, class... Args>
        requires arg_count<non_empty_args, Args...>
    [[nodiscard]] constexpr auto operator()(std::reference_wrapper<Range> const & range, Args &&... args) const
      noexcept(noexcept(operator()(radr::borrow(static_cast<Range &>(range)), std::forward<Args>(args)...)))
    {
        static_assert(std::ranges::forward_range<Range>, RADR_ASSERTSTRING_NOBORROW_SINGLEPASS);

        return operator()(radr::borrow(static_cast<Range &>(range)), std::forward<Args>(args)...);
    }

    template <std::ranges::input_range Range, class... Args>
        requires arg_count<non_empty_args, Args...>
    [[nodiscard]] constexpr auto operator()(std::reference_wrapper<Range> && range, Args &&... args) const
      noexcept(noexcept(operator()(radr::borrow(static_cast<Range &>(range)), std::forward<Args>(args)...)))
    {
        static_assert(std::ranges::forward_range<Range>, RADR_ASSERTSTRING_NOBORROW_SINGLEPASS);

        return operator()(radr::borrow(static_cast<Range &>(range)), std::forward<Args>(args)...);
    }
};

template <bool non_empty_args>
struct pipe_fwd_base<void, non_empty_args>
{};

template <typename CoroFn, typename BorrowFn, bool closure = false>
struct pipe_with_args_fn : pipe_input_base<CoroFn, true>, pipe_fwd_base<BorrowFn, true>
{
    constexpr pipe_with_args_fn(auto &&...) noexcept {}

    using pipe_input_base<CoroFn, true>::operator();
    using pipe_fwd_base<BorrowFn, true>::operator();

    template <class... Args>
        requires(arg_count<true, Args...> && closure == false)
    [[nodiscard]] constexpr auto operator()(Args &&... args) const
      noexcept((std::is_nothrow_constructible_v<std::decay_t<Args>, Args> && ...))
    {
        static_assert((std::constructible_from<std::decay_t<Args>, Args> && ...));

        return range_adaptor_closure_t{
          detail::bind_back(pipe_with_args_fn<CoroFn, BorrowFn, true>{}, std::forward<Args>(args)...)};
    }
};

template <typename CoroFn, typename BorrowFn>
pipe_with_args_fn(CoroFn, BorrowFn) -> pipe_with_args_fn<CoroFn, BorrowFn>;

template <typename CoroFn, typename BorrowFn>
struct pipe_without_args_fn :
  pipe_input_base<CoroFn, false>,
  pipe_fwd_base<BorrowFn, false>,
  range_adaptor_closure<pipe_without_args_fn<CoroFn, BorrowFn>>
{
    constexpr pipe_without_args_fn() = default;
    constexpr pipe_without_args_fn(auto &&...) noexcept {}

    using pipe_input_base<CoroFn, false>::operator();
    using pipe_fwd_base<BorrowFn, false>::operator();

    template <std::ranges::input_range WrappedRange>
        requires std::invocable<pipe_without_args_fn const &, std::reference_wrapper<WrappedRange> const &>
    [[nodiscard]] friend constexpr decltype(auto)
    operator|(std::reference_wrapper<WrappedRange> const & lhs, pipe_without_args_fn const & closure) noexcept(
      std::is_nothrow_invocable_v<pipe_without_args_fn const &, std::reference_wrapper<WrappedRange> const &>)
    {
        // this does not unpack the reference_wrapper, it just forwards to another overload that does
        return std::invoke(closure, lhs);
    }
};

template <typename CoroFn, typename BorrowFn>
pipe_without_args_fn(CoroFn, BorrowFn) -> pipe_without_args_fn<CoroFn, BorrowFn>;

} // namespace radr::detail
