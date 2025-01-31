// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <functional>

namespace radr::detail
{

#ifdef __cpp_lib_bind_back

using std::bind_back;

#else

template <class Op, class Indices, class... BoundArgs>
struct perfect_forward_impl;

template <class Op, size_t... Idx, class... BoundArgs>
struct perfect_forward_impl<Op, std::index_sequence<Idx...>, BoundArgs...>
{
private:
    std::tuple<BoundArgs...> bound_args_;

public:
    template <class... Args, class = std::enable_if_t<std::is_constructible_v<std::tuple<BoundArgs...>, Args &&...>>>
    explicit constexpr perfect_forward_impl(Args &&... bound_args) : bound_args_(std::forward<Args>(bound_args)...)
    {}

    perfect_forward_impl(perfect_forward_impl const &) = default;
    perfect_forward_impl(perfect_forward_impl &&)      = default;

    perfect_forward_impl & operator=(perfect_forward_impl const &) = default;
    perfect_forward_impl & operator=(perfect_forward_impl &&)      = default;

    template <class... Args, class = std::enable_if_t<std::is_invocable_v<Op, BoundArgs &..., Args...>>>
    constexpr auto operator()(Args &&... args) & noexcept(noexcept(Op()(std::get<Idx>(bound_args_)...,
                                                                        std::forward<Args>(args)...)))
      -> decltype(Op()(std::get<Idx>(bound_args_)..., std::forward<Args>(args)...))
    {
        return Op()(std::get<Idx>(bound_args_)..., std::forward<Args>(args)...);
    }

    template <class... Args, class = std::enable_if_t<!std::is_invocable_v<Op, BoundArgs &..., Args...>>>
    auto operator()(Args &&...) & = delete;

    template <class... Args, class = std::enable_if_t<std::is_invocable_v<Op, BoundArgs const &..., Args...>>>
    constexpr auto operator()(Args &&... args) const & noexcept(noexcept(Op()(std::get<Idx>(bound_args_)...,
                                                                              std::forward<Args>(args)...)))
      -> decltype(Op()(std::get<Idx>(bound_args_)..., std::forward<Args>(args)...))
    {
        return Op()(std::get<Idx>(bound_args_)..., std::forward<Args>(args)...);
    }

    template <class... Args, class = std::enable_if_t<!std::is_invocable_v<Op, BoundArgs const &..., Args...>>>
    auto operator()(Args &&...) const & = delete;

    template <class... Args, class = std::enable_if_t<std::is_invocable_v<Op, BoundArgs..., Args...>>>
    constexpr auto operator()(Args &&... args) && noexcept(noexcept(Op()(std::get<Idx>(std::move(bound_args_))...,
                                                                         std::forward<Args>(args)...)))
      -> decltype(Op()(std::get<Idx>(std::move(bound_args_))..., std::forward<Args>(args)...))
    {
        return Op()(std::get<Idx>(std::move(bound_args_))..., std::forward<Args>(args)...);
    }

    template <class... Args, class = std::enable_if_t<!std::is_invocable_v<Op, BoundArgs..., Args...>>>
    auto operator()(Args &&...) && = delete;

    template <class... Args, class = std::enable_if_t<std::is_invocable_v<Op, BoundArgs const..., Args...>>>
    constexpr auto operator()(Args &&... args) const && noexcept(noexcept(Op()(std::get<Idx>(std::move(bound_args_))...,
                                                                               std::forward<Args>(args)...)))
      -> decltype(Op()(std::get<Idx>(std::move(bound_args_))..., std::forward<Args>(args)...))
    {
        return Op()(std::get<Idx>(std::move(bound_args_))..., std::forward<Args>(args)...);
    }

    template <class... Args, class = std::enable_if_t<!std::is_invocable_v<Op, BoundArgs const..., Args...>>>
    auto operator()(Args &&...) const && = delete;
};

// perfect_forward implements a perfect-forwarding call wrapper as explained in [func.require].
template <class Op, class... Args>
using perfect_forward = perfect_forward_impl<Op, std::index_sequence_for<Args...>, Args...>;

template <std::size_t NBound, class = std::make_index_sequence<NBound>>
struct bind_back_op;

template <std::size_t NBound, std::size_t... Ip>
struct bind_back_op<NBound, std::index_sequence<Ip...>>
{
    template <class Fn, class BoundArgs, class... Args>
    constexpr auto operator()(Fn && f, BoundArgs && bound_args, Args &&... args) const
      noexcept(noexcept(std::invoke(std::forward<Fn>(f),
                                    std::forward<Args>(args)...,
                                    std::get<Ip>(std::forward<BoundArgs>(bound_args))...)))
        -> decltype(std::invoke(std::forward<Fn>(f),
                                std::forward<Args>(args)...,
                                std::get<Ip>(std::forward<BoundArgs>(bound_args))...))
    {
        return std::invoke(std::forward<Fn>(f),
                           std::forward<Args>(args)...,
                           std::get<Ip>(std::forward<BoundArgs>(bound_args))...);
    }
};

template <class Fn, class BoundArgs>
struct bind_back_t : perfect_forward<bind_back_op<std::tuple_size_v<BoundArgs>>, Fn, BoundArgs>
{
    using perfect_forward<bind_back_op<std::tuple_size_v<BoundArgs>>, Fn, BoundArgs>::perfect_forward;
};

template <class Fn,
          class... Args,
          class = std::enable_if_t<std::is_constructible_v<std::decay_t<Fn>, Fn> &&
                                   std::is_move_constructible_v<std::decay_t<Fn>> &&
                                   (std::is_constructible_v<std::decay_t<Args>, Args> && ...) &&
                                   (std::is_move_constructible_v<std::decay_t<Args>> && ...)>>
constexpr auto bind_back(Fn && f, Args &&... args) noexcept(noexcept(
  bind_back_t<std::decay_t<Fn>, std::tuple<std::decay_t<Args>...>>(std::forward<Fn>(f),
                                                                   std::forward_as_tuple(std::forward<Args>(args)...))))
  -> decltype(bind_back_t<std::decay_t<Fn>, std::tuple<std::decay_t<Args>...>>(
    std::forward<Fn>(f),
    std::forward_as_tuple(std::forward<Args>(args)...)))
{
    return bind_back_t<std::decay_t<Fn>, std::tuple<std::decay_t<Args>...>>(
      std::forward<Fn>(f),
      std::forward_as_tuple(std::forward<Args>(args)...));
}

#endif

} // namespace radr::detail
