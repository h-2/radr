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

#include <ranges>

namespace radr
{

inline constexpr auto begin = []<std::ranges::range Rng>(Rng && rng)
{
    if constexpr (std::ranges::contiguous_range<Rng>)
        return std::to_address(std::ranges::begin(std::forward<Rng>(rng)));
    else
        return std::ranges::begin(std::forward<Rng>(rng));
};

inline constexpr auto end = []<std::ranges::range Rng>(Rng && rng)
{
    if constexpr (std::ranges::contiguous_range<Rng> && std::ranges::sized_range<Rng>)
        return std::to_address(std::ranges::begin(std::forward<Rng>(rng))) + std::ranges::size(rng);
    // else if constexpr ((!std::ranges::common_range<Rng>) &&
    //                    std::ranges::random_access_range<Rng> &&
    //                    std::ranges::sized_range<Rng>)
    //     return std::ranges::begin(std::forward<Rng>(rng)) + std::ranges::size(rng);
    else
        return std::ranges::end(std::forward<Rng>(rng));
};

template <typename T>
using iterator_t = decltype(radr::begin(std::declval<T &>()));

template <typename T>
using sentinel_t = decltype(radr::end(std::declval<T &>()));

} // namespace radr
