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

#include <concepts>
#include <functional>
#include <iterator>
#include <ranges>

#include "bind_back.hpp"
#include "fwd.hpp"

#define RADR_ASSERTSTRING_RVALUE                                                                                       \
    "RADR adaptors do not accept lvalues of containers unless the indirection is made explicit. To do so,  "           \
    "wrap it in std::ref(), e.g.:\nauto a = std::ref(lvalue) | radr::take(3);"

#define RADR_ASSERTSTRING_CONST_ITERABLE                                                                               \
    "RADR adaptors created on forward ranges require those ranges to be radr::const_iterable, i.e. they need to "      \
    "provide ´.begin() const`.\n"                                                                                     \
    "FIX: do not mix std:: adaptors and radr:: adaptors.\n"                                                            \
    "WORKAROUND: non-conforming ranges can also be downgraded to single-pass, e.g.\n"                                  \
    "auto a = your_range | std::views::filter(/**/) | radr::to_single_pass | radr::take(3);\n "                        \
    "Here, std::views::filter is non-conforming because not const-iterable, so radr's multi-pass take doesn't "        \
    "accept it, but radr's single-pass take does."

#define RADR_ASSERTSTRING_MOVABLE "RADR adaptors on single-pass ranges requires those ranges to be std::movable."

#define RADR_ASSERTSTRING_COPYABLE                                                                                     \
    "RADR adaptors created on rvalues of forward ranges require those ranges to be std::copyable.\n"                   \
    "FIX: do not mix std:: adaptors and radr:: adaptors.\n"                                                            \
    "WORKAROUND: non-conforming ranges can also be downgraded to single-pass, e.g.\n"                                  \
    "auto a = std::vector{1,2,3} | std::views::transform(/**/) | radr::to_single_pass | radr::take(3);\n "             \
    "Here, std::views::transform is non-conforming because not copyable, so radr's multi-pass take doesn't "           \
    "accept it, but radr's single-pass take does."

#define RADR_ASSERTSTRING_NOBORROW_SINGLEPASS                                                                          \
    "RADR adaptors only borrow from (complete) forward ranges. Single-pass input ranges can only be adapted "          \
    "by moving them into the adaptor, e.g.\nauto a = std::move(streamrange) | radr::take(3);"

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

struct empty_t
{
    constexpr empty_t() noexcept = default;
    constexpr empty_t(auto &&) noexcept {}

    constexpr friend bool operator==(empty_t, empty_t) noexcept = default;
};

//=============================================================================
// Range adaptor closure
//=============================================================================

#if defined(__GLIBCXX__)
#    if defined(__cpp_lib_ranges) && (__cpp_lib_ranges >= 202202L)
// GCC >= 14 and C++23; unfortunately libc++ erronously defines the macro without providing the CRTP base
template <typename T>
using range_adaptor_closure = std::ranges::range_adaptor_closure<T>;
#    else

// GCC < 14: _RangeAdaptorClosure is a type
template <typename T, typename Derived>
T rac_t();

// GCC >= 14 and C++20: _RangeAdaptorClosure is a template
template <template <typename> typename T, typename Derived>
T<Derived> rac_t();

template <typename T>
    requires(std::is_class_v<T> && std::same_as<T, std::remove_cv_t<T>>)
struct range_adaptor_closure :
  decltype(rac_t<std::ranges::views::__adaptor::_RangeAdaptorClosure, range_adaptor_closure<T>>())
{};
#    endif
#elif defined(_LIBCPP_VERSION)
#    if _LIBCPP_VERSION >= 190000
template <typename T>
using range_adaptor_closure = std::ranges::__range_adaptor_closure<T>;
#    else
template <typename T>
using range_adaptor_closure = std::__range_adaptor_closure<T>;
#    endif
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

    template <std::ranges::input_range WrappedRange>
        requires std::invocable<Fn const &, std::reference_wrapper<WrappedRange> &>
    [[nodiscard]] friend constexpr decltype(auto)
    operator|(std::reference_wrapper<WrappedRange> const & lhs, range_adaptor_closure_t const & closure) noexcept(
      std::is_nothrow_invocable_v<Fn const &, std::reference_wrapper<WrappedRange> &>)
    {
        return std::invoke(static_cast<Fn const &>(closure), lhs);
    }
};

//=============================================================================
// concepts
//=============================================================================

template <typename T, typename T2>
concept decays_to = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<T2>>;

template <typename T, typename T2>
concept different_from = !decays_to<T, T2>;

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

template <class _Ip, class _Sp, class _CIp, class _CSp, borrowing_rad_kind _Kp>
struct tuple_like_impl<borrowing_rad<_Ip, _Sp, _CIp, _CSp, _Kp>> : std::true_type
{};

template <class _Tp>
concept tuple_like = tuple_like_impl<std::remove_cvref_t<_Tp>>::value;

template <class _Tp>
concept pair_like = tuple_like<_Tp> && std::tuple_size<std::remove_cvref_t<_Tp>>::value == 2;

//=============================================================================
// tuple_like
//=============================================================================

// template <typename T>
// struct const_bounds
// {
//     using it_type  = std::nullptr_t;
//     using sen_type = std::nullptr_t;
// };
//
// template <typename T>
//     requires std::ranges::forward_range<T const>
// struct const_bounds<T>
// {
//     using it_type  = std::ranges::iterator_t<T const>;
//     using sen_type = std::ranges::sentinel_t<T const>;
// };
//
// template <typename Range>
// using const_it_or_nullptr_t = typename const_bounds<std::remove_reference_t<Range>>::it_type;
//
// template <typename Range>
// using const_sen_or_nullptr_t = typename const_bounds<std::remove_reference_t<Range>>::sen_type;

//=============================================================================
// add_const_t
//=============================================================================

template <typename T>
using add_const_t = std::conditional_t<
  std::is_same_v<std::remove_cvref_t<T> &, T>,
  std::remove_cvref_t<T> const &,
  std::conditional_t<std::is_same_v<std::remove_cvref_t<T> &&, T>, std::remove_cvref_t<T> const &&, T const>>;

static_assert(std::same_as<add_const_t<int>, int const>);
static_assert(std::same_as<add_const_t<int &>, int const &>);
static_assert(std::same_as<add_const_t<int &&>, int const &&>);
static_assert(std::same_as<add_const_t<int const>, int const>);
static_assert(std::same_as<add_const_t<int const &>, int const &>);
static_assert(std::same_as<add_const_t<int const &&>, int const &&>);
static_assert(std::same_as<add_const_t<int *>, int * const>);

//=============================================================================
// ptr_to_const_ptr
//=============================================================================

template <typename T>
    requires std::is_pointer_v<T>
using ptr_to_const_ptr_t = std::remove_pointer_t<T> const *;

template <typename T>
    requires std::is_pointer_v<T>
constexpr ptr_to_const_ptr_t<T> ptr_to_const_ptr(T ptr) noexcept
{
    return ptr;
}

} // namespace radr::detail

#define RADR_STR(s)          #s
#define RADR_BUG(file, line) "RADR library BUG in " RADR_STR(file) ":" RADR_STR(line) "; PLEASE REPORT THIS!"
