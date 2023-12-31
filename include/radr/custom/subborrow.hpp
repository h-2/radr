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

#include <iterator>
#include <ranges>

#include "../detail/detail.hpp"
#include "../rad_util/borrowing_rad.hpp"
#include "../range_access.hpp"
#include "tags.hpp"

namespace radr::custom
{

//=============================================================================
// tag_invoke overloads
//=============================================================================

template <const_borrowable_range URange, detail::is_iterator_of<URange> It_, typename Sen_>
auto tag_invoke(subborrow_tag, URange &&, It_ const b, Sen_ const e)
{
    static_assert(detail::one_of<Sen_,
                                 // all of the following can be different,
                                 // e.g. URange is contiguous non-common with non-pointer iterator but Sen_ == It_
                                 It_,
                                 std::ranges::iterator_t<URange>,
                                 std::ranges::sentinel_t<URange>,
                                 iterator_t<URange>,
                                 sentinel_t<URange>>,
                  "The sentinel's type must be the same as the range's sentinel or iterator type.");

    using It       = iterator_t<URange>; // != Iter iff URange is contiguous and Iter is not pointer
    using ConstIt  = const_iterator_t<URange>;
    using Sen      = std::conditional_t<std::same_as<It_, Sen_>, It, sentinel_t<URange>>;
    using ConstSen = std::conditional_t<std::same_as<It_, Sen_>, ConstIt, const_sentinel_t<URange>>;

    // contiguous to pointer
    if constexpr (std::contiguous_iterator<It> && std::sized_sentinel_for<Sen, It>)
    {
        return borrowing_rad<It, It, ConstIt, ConstIt>{std::to_address(b), std::to_address(b) + (e - b)};
    }
    // RA+sized to common
    else if constexpr (std::random_access_iterator<It> && std::sized_sentinel_for<Sen, It>)
    {
        return borrowing_rad<It, It, ConstIt, ConstIt>{b, b + (e - b)};
    }
    else
    {
        return borrowing_rad<It, Sen, ConstIt, ConstSen>{b, e};
    }
}

/*!\brief Create a borrowed range from the iterator-sentinel pair (b,e) and size s.
 *
 * \attention If s is not the actual size of (b, e) the behaviour is undefined.
 */
template <const_borrowable_range URange, detail::is_iterator_of<URange> It_, typename Sen_>
auto tag_invoke(subborrow_tag, URange &&, It_ const b, Sen_ const e, size_t const s)
{
    static_assert(detail::one_of<Sen_,
                                 // all of the following can be different,
                                 // e.g. URange is contiguous non-common with non-pointer iterator but Sen_ == It_
                                 It_,
                                 std::ranges::iterator_t<URange>,
                                 std::ranges::sentinel_t<URange>,
                                 iterator_t<URange>,
                                 sentinel_t<URange>>,
                  "The sentinel's type must be the same as the range's sentinel or iterator type.");

    using It       = iterator_t<URange>; // != Iter iff URange is contiguous and Iter is not pointer
    using ConstIt  = const_iterator_t<URange>;
    using Sen      = std::conditional_t<std::same_as<It_, Sen_>, It, sentinel_t<URange>>;
    using ConstSen = std::conditional_t<std::same_as<It_, Sen_>, ConstIt, const_sentinel_t<URange>>;

    if constexpr (std::sized_sentinel_for<Sen_, It_>)
    {
        assert(int64_t(e - b) == int64_t(s));
    }

    // contiguous to pointer
    if constexpr (std::contiguous_iterator<It>)
    {
        (void)e;
        return borrowing_rad<It, It, ConstIt, ConstIt>{std::to_address(b), std::to_address(b) + s};
    }
    // RA to common
    else if constexpr (std::random_access_iterator<It>)
    {
        (void)e;
        return borrowing_rad<It, It, ConstIt, ConstIt>{b, b + s};
    }
    else
    {
        return borrowing_rad<It, Sen, ConstIt, ConstSen, borrowing_rad_kind::sized>{b, e, s};
    }
}

template <const_borrowable_range URange, detail::is_iterator_of<URange> It, std::sized_sentinel_for<It> Sen>
    requires(std::contiguous_iterator<It> && std::same_as<std::iter_reference_t<URange>, char const &>)
auto tag_invoke(subborrow_tag, URange &&, It const b, Sen const e)
{
    return std::string_view{b, e};
}

template <const_borrowable_range URange, detail::is_iterator_of<URange> It, typename Sen>
    requires(std::contiguous_iterator<It> && std::same_as<std::iter_reference_t<URange>, char const &>)
auto tag_invoke(subborrow_tag, URange &&, It const b, Sen const, size_t const s)
{
    return std::string_view{b, s};
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
  [] <const_borrowable_range URange, detail::is_iterator_of<URange> It, typename Sen>
      (URange && urange, It const b, Sen const e)
  {
      return tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e);
  },
  [] <const_borrowable_range URange, detail::is_iterator_of<URange> It, typename Sen>
      (URange && urange, It const b, Sen const e, size_t const s)
  {
      return tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e, s);
  },
  [] <const_borrowable_range URange>
      (URange && urange, size_t const start, size_t const end)
     requires (std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
  {
      size_t const e = std::min<size_t>(end, std::ranges::size(urange));
      size_t const b = std::min<size_t>(start, e);

      return tag_invoke(custom::subborrow_tag{},
                        std::forward<URange>(urange),
                        radr::begin(urange) + b,
                        radr::begin(urange) + e);
  }
};

inline constexpr auto borrow = detail::overloaded{
  [] <const_borrowable_range URange> (URange && urange)
  {
      if constexpr (std::ranges::sized_range<URange>)
      {
          return tag_invoke(custom::subborrow_tag{},
                            std::forward<URange>(urange),
                            radr::begin(urange),
                            radr::end(urange),
                            std::ranges::size(urange));
      }
      else
      {
          return tag_invoke(custom::subborrow_tag{},
                            std::forward<URange>(urange),
                            radr::begin(urange),
                            radr::end(urange));
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
