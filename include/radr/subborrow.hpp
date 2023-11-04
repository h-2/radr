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

#include <ranges>

#include "borrowing_rad.hpp"
#include "detail/detail.hpp"
#include "tags.hpp"

namespace radr::custom
{

//=============================================================================
// tag_invoke overloads
//=============================================================================

template <const_borrowable_range URange, typename Sen>
auto tag_invoke(subborrow_tag, URange &&, std::ranges::iterator_t<URange> const b, Sen const e)
{
    // contiguous to pointer
    if constexpr (std::contiguous_iterator<std::ranges::iterator_t<URange>> &&
                  std::sized_sentinel_for<Sen, std::ranges::iterator_t<URange>>)
    {
        using It      = decltype(std::to_address(b));
        using ConstIt = std::remove_pointer_t<It> const *;
        return borrowing_rad<It, It, ConstIt, ConstIt>{std::to_address(b), std::to_address(b) + (e - b)};
    }
    // RA+sized to common
    else if constexpr (std::random_access_iterator<std::ranges::iterator_t<URange>> &&
                       std::sized_sentinel_for<Sen, std::ranges::iterator_t<URange>>)
    {
        using It      = std::ranges::iterator_t<URange>;
        using ConstIt = std::ranges::iterator_t<std::remove_reference_t<URange> const>;
        return borrowing_rad<It, It, ConstIt, ConstIt>{b, b + (e - b)};
    }
    else
    {
        static_assert(std::same_as<Sen, std::ranges::sentinel_t<URange>> ||
                        std::same_as<Sen, std::ranges::iterator_t<URange>>,
                      "TODO");
        using It       = std::ranges::iterator_t<URange>;
        using ConstIt  = std::ranges::iterator_t<std::remove_reference_t<URange> const>;
        using ConstSen = std::conditional_t<std::same_as<Sen, std::ranges::iterator_t<URange>>,
                                            ConstIt,
                                            std::ranges::iterator_t<std::remove_reference_t<URange> const>>;

        return borrowing_rad<It, Sen, ConstIt, ConstSen>{b, e};
    }
}

template <const_borrowable_range URange, typename Sen>
auto tag_invoke(subborrow_tag, URange &&, std::ranges::iterator_t<URange> const b, Sen const e, size_t const s)
{
    // contiguous to pointer
    if constexpr (std::contiguous_iterator<std::ranges::iterator_t<URange>>)
    {
        using It      = decltype(std::to_address(b));
        using ConstIt = std::remove_pointer_t<It> const *;
        (void)e;
        return borrowing_rad<It, It, ConstIt, ConstIt>{std::to_address(b), std::to_address(b) + s};
    }
    // RA+sized to common
    else if constexpr (std::random_access_iterator<std::ranges::iterator_t<URange>>)
    {
        using It      = std::ranges::iterator_t<URange>;
        using ConstIt = std::ranges::iterator_t<std::remove_reference_t<URange> const>;
        (void)e;
        return borrowing_rad<It, It, ConstIt, ConstIt>{b, b + s};
    }
    else
    {
        static_assert(std::same_as<Sen, std::ranges::sentinel_t<URange>> ||
                        std::same_as<Sen, std::ranges::iterator_t<URange>>,
                      "TODO");
        using It       = std::ranges::iterator_t<URange>;
        using ConstIt  = std::ranges::iterator_t<std::remove_reference_t<URange> const>;
        using ConstSen = std::conditional_t<std::same_as<Sen, std::ranges::iterator_t<URange>>,
                                            ConstIt,
                                            std::ranges::iterator_t<std::remove_reference_t<URange> const>>;

        if constexpr (std::sized_sentinel_for<Sen, It>)
        {
            assert(e - b == s);
        }

        return borrowing_rad<It, Sen, ConstIt, ConstSen, borrowing_rad_kind::sized>{b, e, s};
    }
}

template <const_borrowable_range URange, typename Sen>
    requires(std::ranges::contiguous_range<URange> && std::ranges::sized_range<URange> &&
             std::same_as<std::ranges::range_reference_t<URange>, char const &>)
auto tag_invoke(subborrow_tag, URange &&, std::ranges::iterator_t<URange> const b, Sen const e)
{
    return std::string_view{b, e};
}

template <const_borrowable_range URange, typename Sen>
    requires(std::ranges::contiguous_range<URange> && std::ranges::sized_range<URange> &&
             std::same_as<std::ranges::range_reference_t<URange>, char const &>)
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
  [] <const_borrowable_range URange, typename Sen> (URange && urange,
                                                    std::ranges::iterator_t<URange> const b,
                                                    Sen const e)
  {
      return tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e);
  },
  [] <const_borrowable_range URange, typename Sen> (URange && urange,
                                                    std::ranges::iterator_t<URange> const b,
                                                    Sen const e,
                                                    size_t const s)
  {
      return tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e, s);
  },
  [] <const_borrowable_range URange> (URange && urange, size_t const start, size_t const end)
     requires (std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
  {
      size_t const e = std::min<size_t>(end, std::ranges::size(urange));
      size_t const b = std::min<size_t>(start, e);

      return tag_invoke(custom::subborrow_tag{},
                        std::forward<URange>(urange),
                        std::ranges::begin(urange) + b,
                        std::ranges::begin(urange) + e);
  }
};

inline constexpr auto borrow = detail::overloaded{
  [] <const_borrowable_range URange> (URange && urange)
  {
      if constexpr (std::ranges::sized_range<URange>)
      {
          return tag_invoke(custom::subborrow_tag{},
                            std::forward<URange>(urange),
                            std::ranges::begin(urange),
                            std::ranges::end(urange),
                            std::ranges::size(urange));
      }
      else
      {
          return tag_invoke(custom::subborrow_tag{},
                            std::forward<URange>(urange),
                            std::ranges::begin(urange),
                            std::ranges::end(urange));
      }
  },
  [] <const_borrowable_range URange> (URange && urange)
    requires (std::ranges::borrowed_range<std::remove_cvref_t<URange>> && std::copyable<std::remove_cvref_t<URange>>)
  {
      return std::forward<URange>(urange);
  }
};
// clang-format on

//TODO nothing uses this at the moment

inline constexpr auto range_fwd = []<std::ranges::range Range>(Range && range) -> decltype(auto)
    requires const_borrowable_range<Range> || movable_range<Range>
{
    if constexpr (const_borrowable_range<Range>)
        return borrow(std::forward<Range>(range));
    else
        return std::forward<Range>(range);
};

} // namespace radr

#define RADR_FWD(R) radr::range_fwd(std::forward<decltype(R)>(R))
