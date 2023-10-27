// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 Hannes Hauswedell
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <concepts>
#include <ranges>

#include "detail/detail.hpp"

namespace radr
{

/*!\brief Ranges that are movable in O(1).
 * \details
 *
 * TODO do we exclude std::array and std::initializer_list?
 */
template <typename T>
concept movable_range = std::ranges::input_range<T> && std::movable<T>;

template <typename Range>
concept unqualified_forward_range =
  std::ranges::forward_range<Range> && std::same_as<Range, std::remove_cvref_t<Range>>;

template <typename Range>
concept const_iterable_range =
  std::ranges::forward_range<Range> && std::ranges::forward_range<detail::add_const_t<Range>>;

template <typename Range>
concept const_symmetric_range =
  const_iterable_range<Range> &&
  std::same_as<std::ranges::iterator_t<Range>, std::ranges::iterator_t<detail::add_const_t<Range>>> &&
  std::same_as<std::ranges::sentinel_t<Range>, std::ranges::sentinel_t<detail::add_const_t<Range>>>;

/*
template <typename Range>
concept complete_forward_range = const_iterable_range<Range> && std::semiregular<Range>;*/

template <typename Range>
concept const_borrowable_range = const_iterable_range<Range> && std::ranges::borrowed_range<Range>;

} // namespace radr

namespace radr::detail
{

template <typename Range>
concept const_borrowable_unqual = const_borrowable_range<Range> && unqualified_forward_range<Range>;

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
} // namespace radr::detail
