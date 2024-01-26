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

#include "concepts.hpp"
#include "detail/basic_const_iterator.hpp"

namespace radr::detail
{

inline constexpr auto cbegin_impl = []<const_borrowable_range Rng>(Rng && rng)
{
    if constexpr (constant_range<detail::add_const_t<Rng>>)
        return std::ranges::begin(std::as_const(rng));
    else
        return make_const_iterator(std::ranges::begin(std::as_const(rng)));
};

inline constexpr auto cend_impl = []<const_borrowable_range Rng>(Rng && rng)
{
    if constexpr (constant_range<detail::add_const_t<Rng>>)
        return std::ranges::end(std::as_const(rng));
    else
        return make_const_sentinel(std::ranges::end(std::as_const(rng)));
};

} // namespace radr::detail

namespace radr
{

inline constexpr auto begin = []<std::ranges::range Rng>(Rng && rng)
{
    if constexpr (std::ranges::contiguous_range<Rng> && std::ranges::sized_range<Rng>)
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

inline constexpr auto cbegin = []<const_borrowable_range Rng>(Rng && rng)
{
    if constexpr (std::ranges::contiguous_range<Rng> && std::ranges::sized_range<Rng>)
        return detail::ptr_to_const_ptr(std::to_address(std::ranges::begin(rng)));
    else
        return detail::cbegin_impl(rng);
};

inline constexpr auto cend = []<const_borrowable_range Rng>(Rng && rng)
{
    if constexpr (std::ranges::contiguous_range<Rng> && std::ranges::sized_range<Rng>)
        return cbegin(rng) + std::ranges::size(rng);
    // else if constexpr ((!std::ranges::common_range<Rng>) &&
    //                    std::ranges::random_access_range<Rng> &&
    //                    std::ranges::sized_range<Rng>)
    //     return std::ranges::cbegin(std::forward<Rng>(rng)) + std::ranges::size(rng);
    else
        return detail::cend_impl(rng);
};

template <typename T>
using iterator_t = decltype(radr::begin(std::declval<T &>()));

template <typename T>
using const_iterator_t = decltype(radr::cbegin(std::declval<T &>()));

template <typename T>
using sentinel_t = decltype(radr::end(std::declval<T &>()));

template <typename T>
using const_sentinel_t = decltype(radr::cend(std::declval<T &>()));

} // namespace radr

namespace radr::detail
{

template <typename T>
using std_const_iterator_t = decltype(detail::cbegin_impl(std::declval<T &>()));

template <typename T>
using std_const_sentinel_t = decltype(detail::cend_impl(std::declval<T &>()));

template <typename It, typename Range>
concept is_iterator_of =
  one_of<It, iterator_t<Range>, const_iterator_t<Range>, std::ranges::iterator_t<Range>, std_const_iterator_t<Range>>;

template <typename Sen, typename Range>
concept is_sentinel_of =
  one_of<Sen, sentinel_t<Range>, const_sentinel_t<Range>, std::ranges::sentinel_t<Range>, std_const_sentinel_t<Range>> ||
  (std::ranges::forward_range<Range> && is_iterator_of<Sen, Range>);

} // namespace radr::detail
