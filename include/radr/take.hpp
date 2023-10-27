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

#include "borrowing_rad.hpp"
#include "generator.hpp"

namespace radr
{

template <unqualified_forward_range URange, bool Const>
class take_sentinel
{
    using Base = detail::maybe_const<Const, URange>;

    template <bool Const_>
    using Iter = std::counted_iterator<std::ranges::iterator_t<detail::maybe_const<Const_, URange>>>;

    [[no_unique_address]] std::ranges::sentinel_t<Base> end_ = std::ranges::sentinel_t<Base>();

    template <unqualified_forward_range URange_, bool Const_>
    friend class take_sentinel;

public:
    take_sentinel() = default;

    constexpr explicit take_sentinel(std::ranges::sentinel_t<Base> end) : end_(std::move(end)) {}

    constexpr take_sentinel(take_sentinel<URange, !Const> s)
        requires(Const && std::convertible_to<std::ranges::sentinel_t<URange>, std::ranges::sentinel_t<Base>>)
      : end_(std::move(s.end_))
    {}

    constexpr std::ranges::sentinel_t<Base> base() const { return end_; }

    friend constexpr bool operator==(Iter<Const> const & lhs, take_sentinel const & rhs)
    {
        return lhs.count() == 0 || lhs.base() == rhs.end_;
    }

    template <bool OtherConst = !Const>
        requires std::sentinel_for<std::ranges::sentinel_t<Base>,
                                   std::ranges::iterator_t<detail::maybe_const<OtherConst, URange>>>
    friend constexpr bool operator==(Iter<Const> const & lhs, take_sentinel const & rhs)
    {
        return lhs.count() == 0 || lhs.base() == rhs.end_;
    }
};

inline constexpr auto take_borrow = detail::overloaded{
  []<const_borrowable_range URange>(URange && urange, std::ranges::range_size_t<URange> const n)
  {
      using sz_t = std::ranges::range_size_t<URange>;
      sz_t sz    = n;
      if constexpr (std::ranges::sized_range<URange>)
          sz = std::min<sz_t>(n, std::ranges::size(urange));

      using URangeNoCVRef = std::remove_cvref_t<URange>;

      //TODO double-check this special-casing
      if constexpr (std::ranges::sized_range<URange>)
      {
          using BorrowingRad =
            borrowing_rad<std::counted_iterator<std::ranges::iterator_t<URange>>,
                          std::default_sentinel_t,
                          std::counted_iterator<std::ranges::iterator_t<URangeNoCVRef const>>,
                          std::default_sentinel_t,
                          borrowing_rad_kind::sized>;

          return BorrowingRad{std::counted_iterator<std::ranges::iterator_t<URange>>(std::ranges::begin(urange), sz),
                              std::default_sentinel,
                              sz};
      }
      else
      {
          using BorrowingRad = borrowing_rad<
            std::counted_iterator<std::ranges::iterator_t<URange>>,
            take_sentinel<URangeNoCVRef, const_symmetric_range<URange>>,
            std::counted_iterator<std::ranges::iterator_t<URangeNoCVRef const>>,
            take_sentinel<URangeNoCVRef, true>,
            borrowing_rad_kind::unsized>;

          return BorrowingRad{std::counted_iterator<std::ranges::iterator_t<URange>>(std::ranges::begin(urange), sz),
                              take_sentinel<URangeNoCVRef, const_symmetric_range<URange>>{std::ranges::end(urange)}};
      }
  },
  []<const_borrowable_range URange>(URange && urange, std::ranges::range_size_t<URange> const n) requires(
    std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>){
    return subborrow(std::forward<URange>(urange), 0ull, n);
} // namespace radr
}
;

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

} // namespace radr

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto take = detail::pipe_with_args_fn{take_coro, take_borrow};
} // namespace cpo
} // namespace radr::pipe
