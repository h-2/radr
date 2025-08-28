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
#include <ranges>
#include <type_traits>

#include "detail/detail.hpp"

namespace radr::detail
{

template <typename T>
concept has_type_member_type = requires { typename T::type; };

}

namespace radr
{

/*!\brief Ranges that are movable in O(1).
 * \details
 *
 * TODO do we exclude std::array and std::initializer_list?
 */
template <typename T>
concept movable_range = std::ranges::input_range<T> && std::movable<T>;

template <std::indirectly_readable T>
using iter_const_reference_t = std::common_reference_t<std::iter_value_t<T> const &&, std::iter_reference_t<T>>;

template <class It>
concept constant_iterator =
  std::forward_iterator<It> && std::same_as<iter_const_reference_t<It>, std::iter_reference_t<It>>;

//!\brief A multi-pass range is a forward range that is also const-iterable.
template <typename Range>
concept mp_range = std::ranges::forward_range<Range> && std::ranges::forward_range<detail::add_const_t<Range>>;

//!\brief A range whose iterator_t / sentinel_t are the same types as its const_iterator_t / const_sentinel_t.
template <typename Range>
concept const_symmetric_range =
  mp_range<Range> &&
  std::same_as<std::ranges::iterator_t<Range>, std::ranges::iterator_t<detail::add_const_t<Range>>> &&
  std::same_as<std::ranges::sentinel_t<Range>, std::ranges::sentinel_t<detail::add_const_t<Range>>>;

//!\brief In contrast to std::ranges::constant_range, this also requires const-iterability.
template <class Range>
concept constant_range = const_symmetric_range<Range> && constant_iterator<std::ranges::iterator_t<Range>>;

template <class Range>
concept mutable_range = std::ranges::input_range<Range> && !constant_iterator<std::ranges::iterator_t<Range>>;

//!\brief A multi-pass range that is borrowed.
template <typename Range>
concept borrowed_mp_range = mp_range<Range> && std::ranges::borrowed_range<Range>;

//!\brief A multi-pass range that is borrowed & is a cv-unqualified object type & is semiregular.
template <typename Range>
concept borrowed_mp_range_object =
  borrowed_mp_range<Range> && std::same_as<Range, std::remove_reference_t<Range>> && std::semiregular<std::remove_cvref_t<Range>>;

//!\brief A borrowed_mp_range_object type or std::reference_wrapper around an mp_range.
template <typename Range>
concept explicitly_borrowed_range = borrowed_mp_range_object<Range> || (detail::has_type_member_type<Range> && std::same_as<Range, std::reference_wrapper<typename Range::type>> && mp_range<typename Range::type>);

//!\brief A type that can be efficiently created & copied (nothrow), and is no bigger than three pointers.
template <typename T>
concept small_type = std::regular<T> && std::is_nothrow_default_constructible_v<T> &&
                     std::is_nothrow_copy_constructible_v<T> && sizeof(T) <= 3 * sizeof(ptrdiff_t);

//!\brief The same as std::indirect_unary_invocable, but only requires move-construction instead of copy_construction.
template <class F, class I>
concept weak_indirect_unary_invocable =
  std::indirectly_readable<I> && std::move_constructible<F> && std::invocable<F &, std::iter_value_t<I>> &&
  std::invocable<F &, std::iter_reference_t<I>> &&
  std::common_reference_with<std::invoke_result_t<F &, std::iter_value_t<I>>,
                             std::invoke_result_t<F &, std::iter_reference_t<I>>>;
//TODO implement P2609

template <typename Rng>
concept infinite_mp_range = mp_range<Rng> && std::same_as<std::ranges::sentinel_t<Rng>, std::unreachable_sentinel_t>;

template <typename Rng>
concept safely_indexable_range =
  std::ranges::random_access_range<Rng> && (std::ranges::sized_range<Rng> || infinite_mp_range<Rng>);

} // namespace radr

namespace radr::detail
{


template <class B>
concept boolean_testable = std::convertible_to<B, bool> && requires(B && b) {
    {
        !std::forward<B>(b)
    } -> std::convertible_to<bool>;
};

template <class T, class U>
concept weakly_equality_comparable_with =
  requires(std::remove_reference_t<T> const & t, std::remove_reference_t<U> const & u) {
      {
          t == u
      } -> boolean_testable;
      {
          t != u
      } -> boolean_testable;
      {
          u == t
      } -> boolean_testable;
      {
          u != t
      } -> boolean_testable;
  };

template <class T>
concept weakly_equality_comparable = weakly_equality_comparable_with<T, T>;

template <class T, class... Us>
concept one_of = (std::same_as<T, Us> || ...);

template <typename T>
concept object = std::is_object_v<T>;

} // namespace radr::detail
