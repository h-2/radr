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

namespace radr
{

/*!\brief Ranges that are movable in O(1).
 * \details
 *
 * TODO
 */
template <typename T>
concept adaptable_range = std::ranges::input_range<T> && std::movable<T>;

template <typename T>
concept rad_constraints =
  adaptable_range<T> && std::ranges::forward_range<T> && std::same_as<T, std::remove_cvref_t<T>>;

template <typename Range>
concept const_iterable_range = std::ranges::forward_range<std::remove_reference_t<Range>> &&
                               std::ranges::forward_range<std::remove_reference_t<Range> const>;

template <typename Range>
concept const_symmetric_range = const_iterable_range<Range> &&
                                std::same_as<std::ranges::iterator_t<std::remove_reference_t<Range>>,
                                             std::ranges::iterator_t<std::remove_reference_t<Range> const>> &&
                                std::same_as<std::ranges::sentinel_t<std::remove_reference_t<Range>>,
                                             std::ranges::sentinel_t<std::remove_reference_t<Range> const>>;
} // namespace radr
