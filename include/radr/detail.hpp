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

#define RADR_RVALUE_ASSERTION_STRING                                                                                   \
    "RADR adaptors and coroutines only take arguments as (r)values. If you want to "                                   \
    "adapt an lvalue range, wrap it in radr::range_fwd() or wrap it std::ref() "                                       \
    "and pass it to the respective object in radr::pipe::"

namespace radr::detail
{

template <bool b, typename T>
using maybe_const = std::conditional_t<b, T const, T>;

#if defined(__GLIBCXX__)
template <typename T>
using box = std::ranges::__detail::__box<T>;
#elif defined(_LIBCPP_VERSION)
using box = std::ranges::__copyable_box<T>;
#else
#    error "RADR requires either libstdc++ or libc++ standard libraries."
#endif

template <typename T>
using plus_ref = T &;

template <typename T>
concept can_reference = requires { typename plus_ref<T>; };

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

} // namespace radr::detail
