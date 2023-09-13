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

#include <functional>
#include <ranges>

#include "concepts.hpp"
#include "detail/detail.hpp"
#include "detail/pipe.hpp"
#include "generator.hpp"
#include "tags.hpp"

namespace radr::custom
{

//=============================================================================
// tag_invoke overloads
//=============================================================================

template <std::ranges::borrowed_range URange, typename Sen>
    requires(std::same_as<Sen, std::ranges::sentinel_t<URange>> || std::same_as<Sen, std::ranges::iterator_t<URange>>)
auto tag_invoke(subborrow_tag, URange &&, std::ranges::iterator_t<URange> const b, Sen const e)
{
    using It = std::ranges::iterator_t<URange>;

    static constexpr borrowing_rad_kind kind =
      std::sized_sentinel_for<Sen, It> ? borrowing_rad_kind::sized : borrowing_rad_kind::unsized;

    using ConstIt  = detail::const_it_or_nullptr_t<URange>;
    using ConstSen = std::conditional_t<std::same_as<Sen, std::ranges::iterator_t<URange>>,
                                        detail::const_it_or_nullptr_t<URange>,
                                        detail::const_sen_or_nullptr_t<URange>>;

    return borrowing_rad<It, Sen, ConstIt, ConstSen, kind>{b, e};
}

template <std::ranges::borrowed_range URange, typename Sen>
    requires(std::same_as<Sen, std::ranges::sentinel_t<URange>> || std::same_as<Sen, std::ranges::iterator_t<URange>>)
auto tag_invoke(subborrow_tag, URange &&, std::ranges::iterator_t<URange> const b, Sen const e, size_t const s)
{
    using It       = std::ranges::iterator_t<URange>;
    using ConstIt  = detail::const_it_or_nullptr_t<URange>;
    using ConstSen = std::conditional_t<std::same_as<Sen, std::ranges::iterator_t<URange>>,
                                        detail::const_it_or_nullptr_t<URange>,
                                        detail::const_sen_or_nullptr_t<URange>>;

    if constexpr (std::sized_sentinel_for<Sen, It>)
    {
        assert(e - b == s);
    }

    return borrowing_rad<It, Sen, ConstIt, ConstSen, borrowing_rad_kind::sized>{b, e, s};
}

template <std::ranges::borrowed_range URange, typename Sen>
    requires(std::ranges::contiguous_range<URange> && std::ranges::sized_range<URange> &&
             (std::same_as<Sen, std::ranges::sentinel_t<URange>> || std::same_as<Sen, std::ranges::iterator_t<URange>>))
auto tag_invoke(subborrow_tag, URange &&, std::ranges::iterator_t<URange> const b, Sen const e)
{
    return std::span{b, e};
}

template <std::ranges::borrowed_range URange, typename Sen>
    requires(std::ranges::contiguous_range<URange> && std::ranges::sized_range<URange> &&
             (std::same_as<Sen, std::ranges::sentinel_t<URange>> ||
              std::same_as<Sen, std::ranges::iterator_t<URange>>) &&
             std::same_as<std::ranges::range_reference_t<URange>, char const &>)
auto tag_invoke(subborrow_tag, URange &&, std::ranges::iterator_t<URange> const b, Sen const e)
{
    return std::string_view{b, e};
}

template <std::ranges::borrowed_range URange, typename Sen>
    requires(std::ranges::contiguous_range<URange> && std::ranges::sized_range<URange> &&
             (std::same_as<Sen, std::ranges::sentinel_t<URange>> || std::same_as<Sen, std::ranges::iterator_t<URange>>))
auto tag_invoke(subborrow_tag, URange && urange, std::ranges::iterator_t<URange> const b, Sen const e, size_t)
{
    return tag_invoke(subborrow_tag{}, std::forward<URange>(urange), b, e);
}

// TODO overloads for subrange and empty_view and iota_views that are borrowed
// technically, all RA+sized borrowed ranges from std could have overloads; but likely too much work

} // namespace radr::custom

namespace radr
{

//=============================================================================
// Wrapper function
//=============================================================================

// clang-format off
inline constexpr auto subborrow = detail::overloaded{
  [] <std::ranges::borrowed_range URange, typename Sen>
     (URange && urange, std::ranges::iterator_t<URange> const b, Sen const e)
     requires (std::same_as<Sen, std::ranges::sentinel_t<URange>> || std::same_as<Sen, std::ranges::iterator_t<URange>>)
  {
      return tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e);
  },
  [] <std::ranges::borrowed_range URange, typename Sen>
     (URange && urange, std::ranges::iterator_t<URange> const b, Sen const e, size_t const s)
     requires (std::same_as<Sen, std::ranges::sentinel_t<URange>> || std::same_as<Sen, std::ranges::iterator_t<URange>>)
  {
      return tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e, s);
  },
  [] <std::ranges::borrowed_range URange> (URange && urange, size_t const start, size_t const end)
     requires (std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
  {
      size_t const e = std::min<size_t>(end, std::ranges::size(urange));
      size_t const b = std::min<size_t>(start, e);

      return tag_invoke(custom::subborrow_tag{},
                        std::forward<URange>(urange),
                        std::ranges::begin(urange) + b,
                        std::ranges::begin(urange) + e);
  }};

// clang-format on

} // namespace radr
