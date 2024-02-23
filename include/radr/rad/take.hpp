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

#include "../detail/pipe.hpp"
#include "../generator.hpp"
#include "../rad_util/borrowing_rad.hpp"

namespace radr::detail
{

template <std::forward_iterator Iter, std::sentinel_for<Iter> Sen>
class take_sentinel
{
    [[no_unique_address]] Sen end_{};

    template <std::forward_iterator Iter_, std::sentinel_for<Iter_> Sen_>
    friend class take_sentinel;

public:
    take_sentinel() = default;

    constexpr explicit take_sentinel(Sen end) : end_(std::move(end)) {}

    template <std::forward_iterator OtherIter, std::sentinel_for<OtherIter> OtherSen>
        requires(std::convertible_to<OtherIter, Iter> && std::convertible_to<OtherSen, Sen>)
    constexpr take_sentinel(take_sentinel<OtherIter, OtherSen> s) : end_(std::move(s.end_))
    {}

    constexpr Sen base() const { return end_; }

    friend constexpr bool operator==(std::counted_iterator<Iter> const & lhs, take_sentinel const & rhs)
    {
        return lhs.count() == 0 || lhs.base() == rhs.end_;
    }
};

inline constexpr auto take_borrow = overloaded{
  []<const_borrowable_range URange>(URange && urange, range_size_t_or_size_t<URange> const n)
  {
      using sz_t = range_size_t_or_size_t<URange>;
      sz_t sz    = n;
      if constexpr (std::ranges::sized_range<URange>)
          sz = std::min<sz_t>(n, std::ranges::size(urange));

      if constexpr (std::ranges::sized_range<URange>)
      {
          using BorrowingRad = borrowing_rad<std::counted_iterator<radr::iterator_t<URange>>,
                                             std::default_sentinel_t,
                                             std::counted_iterator<radr::const_iterator_t<URange>>,
                                             std::default_sentinel_t,
                                             borrowing_rad_kind::sized>;

          return BorrowingRad{std::counted_iterator<radr::iterator_t<URange>>(std::ranges::begin(urange), sz),
                              std::default_sentinel,
                              sz};
      }
      else
      {
          using BorrowingRad =
            borrowing_rad<std::counted_iterator<radr::iterator_t<URange>>,
                          take_sentinel<radr::iterator_t<URange>, radr::sentinel_t<URange>>,
                          std::counted_iterator<radr::const_iterator_t<URange>>,
                          take_sentinel<radr::const_iterator_t<URange>, radr::const_sentinel_t<URange>>,
                          borrowing_rad_kind::unsized>;

          return BorrowingRad{std::counted_iterator<radr::iterator_t<URange>>(radr::begin(urange), sz),
                              take_sentinel<radr::iterator_t<URange>, radr::sentinel_t<URange>>{radr::end(urange)}};
      }
  },
  []<const_borrowable_range URange>(URange && urange, range_size_t_or_size_t<URange> const n)
      requires(std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
  { return subborrow(std::forward<URange>(urange), 0ull, n); } // namespace radr
  };

inline constexpr auto take_coro = []<movable_range URange>(URange && urange, std::size_t const n)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);

    // we need to create inner functor so that it can take by value
    return [](auto urange_, std::size_t const n)
             -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        std::size_t i = 0;
        for (auto it = std::ranges::begin(urange_); it != std::ranges::end(urange_) && i < n; ++it, ++i)
            co_yield *it;
    }(std::move(urange), n);
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
inline constexpr auto take = detail::pipe_with_args_fn{detail::take_coro, detail::take_borrow};
} // namespace cpo
} // namespace radr
