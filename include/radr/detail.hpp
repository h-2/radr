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

#include <concepts>
#include <iterator>
#include <ranges>

#include "bind_back.hpp"
#include "fwd.hpp"

#define RADR_RVALUE_ASSERTION_STRING                                                                                   \
    "RADR adaptors and coroutines only take arguments as (r)values. If you want to "                                   \
    "adapt an lvalue range, wrap it in radr::range_fwd() or wrap it std::ref() "                                       \
    "and pass it to the respective object in radr::pipe::"

namespace radr::detail
{

template <bool b, typename T>
using maybe_const = std::conditional_t<b, T const, T>;

template <typename T>
using plus_ref = T &;

template <typename T>
concept can_reference = requires { typename plus_ref<T>; };

template <typename Ip>
concept has_arrow = std::input_iterator<Ip> && (std::is_pointer_v<Ip> || requires(Ip i) { i.operator->(); });

//TODO special cases
template <std::integral T>
constexpr auto to_unsigned_like(T v) noexcept
{
    return static_cast<std::make_unsigned_t<T>>(v);
}

template <typename... Fn>
struct overloaded : Fn...
{
    using Fn::operator()...;
};

template <typename... Fn>
overloaded(Fn...) -> overloaded<Fn...>;

//=============================================================================
// Range adaptor closure
//=============================================================================

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 202202L
template <typename T>
using range_adaptor_closure = std::ranges::range_adaptor_closure<T>;
#elif defined(__GLIBCXX__)
template <typename T>
    requires(std::is_class_v<T> && std::same_as<T, std::remove_cv_t<T>>)
struct range_adaptor_closure : std::ranges::views::__adaptor::_RangeAdaptorClosure
{};
#elif defined(_LIBCPP_VERSION)
template <typename T>
using range_adaptor_closure = std::__range_adaptor_closure<T>;
#else
#    error "RADR requires either libstdc++ or libc++ standard libraries."
#endif

template <class Tp>
concept RangeAdaptorClosure =
  std::derived_from<std::remove_cvref_t<Tp>, range_adaptor_closure<std::remove_cvref_t<Tp>>>;

template <class Fn>
struct range_adaptor_closure_t : Fn, range_adaptor_closure<range_adaptor_closure_t<Fn>>
{
    constexpr explicit range_adaptor_closure_t(Fn && f) : Fn(std::move(f)) {}
};

// #define RADR_SIMPLE_CLOSURE

//=============================================================================
// concepts
//=============================================================================

template <typename T, typename T2>
concept different_from = !std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<T2>>;

template <typename Range>
concept simple_range = std::ranges::range<Range> && std::ranges::range<Range const> &&
                       std::same_as<std::ranges::iterator_t<Range>, std::ranges::iterator_t<Range const>> &&
                       std::same_as<std::ranges::sentinel_t<Range>, std::ranges::sentinel_t<Range const>>;

/*
template <typename T>
using iterator_category_tag = decltype([]()
{
    if constexpr (requires { typename std::ranges::iterator_traits<T>::iterator_category; })
        return typename std::ranges::iterator_traits<T>::iterator_category{};
    else
        std::input_iterator_tag{};
}());

template <typename T>
using iterator_concept_tag = decltype([]()
{
    if constexpr (requires { typename std::ranges::iterator_traits<T>::iterator_concept; })
        return typename std::ranges::iterator_traits<T>::iterator_concept{};
    else
        return iterator_category_tag<T>{};
}());*/

//=============================================================================
// tuple_like
//=============================================================================

template <class _Tp>
struct tuple_like_impl : std::false_type
{};

template <class _T1, class _T2>
struct tuple_like_impl<std::pair<_T1, _T2>> : std::true_type
{};

template <class _Tp, size_t _Size>
struct tuple_like_impl<std::array<_Tp, _Size>> : std::true_type
{};

template <class _Ip, class _Sp, std::ranges::subrange_kind _Kp>
struct tuple_like_impl<std::ranges::subrange<_Ip, _Sp, _Kp>> : std::true_type
{};

template <class _Ip, class _Sp, class _CIp, class _CSp, range_bounds_kind _Kp>
struct tuple_like_impl<range_bounds<_Ip, _Sp, _CIp, _CSp, _Kp>> : std::true_type
{};

template <class _Tp>
concept tuple_like = tuple_like_impl<std::remove_cvref_t<_Tp>>::value;

template <class _Tp>
concept pair_like = tuple_like<_Tp> && std::tuple_size<std::remove_cvref_t<_Tp>>::value == 2;

//=============================================================================
// tuple_like
//=============================================================================

template <typename T>
struct const_bounds
{
    using it_type  = std::nullptr_t;
    using sen_type = std::nullptr_t;
};

template <typename T>
    requires std::ranges::forward_range<T const>
struct const_bounds<T>
{
    using it_type  = std::ranges::iterator_t<T const>;
    using sen_type = std::ranges::sentinel_t<T const>;
};

template <typename Range>
using const_it_or_nullptr_t = typename const_bounds<Range>::it_type;

template <typename Range>
using const_sen_or_nullptr_t = typename const_bounds<Range>::sen_type;

} // namespace radr::detail
