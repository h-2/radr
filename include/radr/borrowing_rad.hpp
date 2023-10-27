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

#include <cassert>
#include <iterator>

#include "concepts.hpp"
#include "detail/detail.hpp"
#include "detail/fwd.hpp"
#include "rad_interface.hpp"

namespace radr::detail
{

template <class From, class To>
concept uses_nonqualification_pointer_conversion =
  std::is_pointer_v<From> && std::is_pointer_v<To> &&
  !std::convertible_to<std::remove_pointer_t<From> (*)[], std::remove_pointer_t<To> (*)[]>;

template <class From, class To>
concept convertible_to_non_slicing =
  std::convertible_to<From, To> && !uses_nonqualification_pointer_conversion<std::decay_t<From>, std::decay_t<To>>;

template <class Pair, class Iter, class Sent>
concept pair_like_convertible_from =
  !std::ranges::range<Pair> && pair_like<Pair> && std::constructible_from<Pair, Iter, Sent> &&
  convertible_to_non_slicing<Iter, std::tuple_element_t<0, Pair>> &&
  std::convertible_to<Sent, std::tuple_element_t<1, Pair>>;

template <class CIter, class CSent, class OtherRange>
concept compatible_contiguous_range =
  std::ranges::contiguous_range<OtherRange> && std::same_as<std::ranges::range_value_t<OtherRange> const *, CIter> &&
  std::same_as<std::ranges::range_value_t<OtherRange> const *, CSent>;
} // namespace radr::detail

namespace radr
{

template <std::forward_iterator    Iter,
          std::sentinel_for<Iter>  Sent,
          std::forward_iterator    CIter,
          std::sentinel_for<CIter> CSent,
          borrowing_rad_kind       Kind =
            std::sized_sentinel_for<Sent, Iter> ? borrowing_rad_kind::sized : borrowing_rad_kind::unsized>
    requires(Kind == borrowing_rad_kind::sized || !std::sized_sentinel_for<Sent, Iter>)
class borrowing_rad : public rad_interface<borrowing_rad<Iter, Sent, CIter, CSent, Kind>>
{
public:
    // Note: this is an internal implementation detail that is public only for internal usage.
    static constexpr bool StoreSize = (Kind == borrowing_rad_kind::sized && !std::sized_sentinel_for<Sent, Iter>);

private:
    static constexpr bool MustProvideSizeAtConstruction = !StoreSize; // just to improve compiler diagnostics
    struct Empty
    {
        constexpr Empty(auto) noexcept {}
    };
    using Size = std::conditional_t<StoreSize, std::make_unsigned_t<std::iter_difference_t<Iter>>, Empty>;

    static constexpr bool const_symmetric = std::same_as<CIter, Iter> && std::same_as<CSent, Sent>;

    static_assert(std::sized_sentinel_for<Sent, Iter> == std::sized_sentinel_for<CSent, CIter>,
                  "The const sentinel shall be a sized sentinel for the const iterator IF AND ONLY IF "
                  "the sentinel is a sized sentinel for the iterator.");
    static_assert(std::convertible_to<Iter, CIter>, "The iterator is not convertible to the const_iterator.");
    static_assert(std::convertible_to<Sent, CSent>, "The sentinel is not convertible to the const_sentinel.");

    [[no_unique_address]] Iter begin_ = Iter();
    [[no_unique_address]] Sent end_   = Sent();
    [[no_unique_address]] Size size_  = 0;

public:
    borrowing_rad() = default;

    //!\brief Iterator, Sentinel
    constexpr borrowing_rad(detail::convertible_to_non_slicing<Iter> auto iter, Sent sent)
        requires MustProvideSizeAtConstruction
      : begin_(std::move(iter)), end_(std::move(sent))
    {}

    //!\brief Iterator, Sentinel, Size
    constexpr borrowing_rad(detail::convertible_to_non_slicing<Iter> auto      iter,
                            Sent                                               sent,
                            std::make_unsigned_t<std::iter_difference_t<Iter>> n)
        requires(Kind == borrowing_rad_kind::sized)
      : begin_(std::move(iter)), end_(std::move(sent)), size_(n)
    {
        if constexpr (std::sized_sentinel_for<Sent, Iter>)
            assert((end_ - begin_) == static_cast<std::iter_difference_t<Iter>>(n));
    }

    //!\brief Range generic
    template <detail::different_from<borrowing_rad> Range>
        requires(std::ranges::borrowed_range<Range> &&
                 detail::convertible_to_non_slicing<std::ranges::iterator_t<Range>, Iter> &&
                 std::convertible_to<std::ranges::sentinel_t<Range>, Sent>)
    constexpr borrowing_rad(Range && range)
        requires(Kind == borrowing_rad_kind::unsized)
      : borrowing_rad(std::ranges::begin(range), std::ranges::end(range))
    {}

    //!\brief Range sized
    template <detail::different_from<borrowing_rad> Range>
        requires(std::ranges::borrowed_range<Range> && std::ranges::sized_range<Range> &&
                 ((detail::convertible_to_non_slicing<std::ranges::iterator_t<Range>, Iter> &&
                   std::convertible_to<std::ranges::sentinel_t<Range>, Sent>) ||
                  detail::compatible_contiguous_range<CIter, CSent, Range>))
    constexpr borrowing_rad(Range && range) : borrowing_rad(range, std::ranges::size(range))
    {}

    //!\brief Range, Size
    template <std::ranges::borrowed_range Range>
        requires(detail::convertible_to_non_slicing<std::ranges::iterator_t<Range>, Iter> &&
                 std::convertible_to<std::ranges::sentinel_t<Range>, Sent> && !std::ranges::random_access_range<Range>)
    constexpr borrowing_rad(Range && range, std::make_unsigned_t<std::iter_difference_t<Iter>> n)
        requires(Kind == borrowing_rad_kind::sized)
      : borrowing_rad(std::ranges::begin(range), std::ranges::end(range), n)
    {}

    //!\brief Range RA, Size
    template <std::ranges::borrowed_range Range>
        requires(detail::convertible_to_non_slicing<std::ranges::iterator_t<Range>, Iter> && std::same_as<Iter, Sent> &&
                 std::ranges::random_access_range<Range> && !detail::compatible_contiguous_range<CIter, CSent, Range>)
    constexpr borrowing_rad(Range && range, std::make_unsigned_t<std::iter_difference_t<Iter>> n)
        requires(Kind == borrowing_rad_kind::sized)
      : borrowing_rad(std::ranges::begin(range), std::ranges::begin(range) + n, n)
    {}

    //!\brief Range contiguous, Size
    template <std::ranges::borrowed_range Range>
        requires(detail::compatible_contiguous_range<CIter, CSent, Range>)
    constexpr borrowing_rad(Range && range, std::make_unsigned_t<std::iter_difference_t<Iter>> n)
        requires(Kind == borrowing_rad_kind::sized)
      : borrowing_rad(std::ranges::data(range), std::ranges::data(range) + n, n)
    {}

    template <detail::different_from<borrowing_rad> Pair>
        requires detail::pair_like_convertible_from<Pair, Iter const &, Sent const &>
    constexpr operator Pair() const
    {
        return Pair(begin_, end_);
    }

    constexpr Iter begin()
        requires(!const_symmetric)
    {
        return begin_;
    }

    constexpr Iter begin() const
        requires(const_symmetric)
    {
        return begin_;
    }

    constexpr CIter begin() const
        requires(!const_symmetric)
    {
        return static_cast<CIter>(begin_);
    }

    constexpr Sent end()
        requires(!const_symmetric)
    {
        return end_;
    }

    constexpr Sent end() const
        requires(const_symmetric)
    {
        return end_;
    }

    constexpr CSent end() const
        requires(!const_symmetric)
    {
        return static_cast<CSent>(end_);
    }

    [[nodiscard]] constexpr bool empty() const { return begin_ == end_; }

    constexpr std::make_unsigned_t<std::iter_difference_t<Iter>> size() const
        requires(Kind == borrowing_rad_kind::sized)
    {
        if constexpr (StoreSize)
            return size_;
        else
            return detail::to_unsigned_like(end_ - begin_);
    }

    [[nodiscard]] constexpr borrowing_rad next(std::iter_difference_t<Iter> n = 1) const &
    {
        auto tmp = *this;
        tmp.advance(n);
        return tmp;
    }

    [[nodiscard]] constexpr borrowing_rad next(std::iter_difference_t<Iter> n = 1) &&
    {
        advance(n);
        return std::move(*this);
    }

    [[nodiscard]] constexpr borrowing_rad prev(std::iter_difference_t<Iter> n = 1) const
        requires std::bidirectional_iterator<Iter>
    {
        auto tmp = *this;
        tmp.advance(-n);
        return tmp;
    }

    constexpr borrowing_rad & advance(std::iter_difference_t<Iter> n)
    {
        if constexpr (std::bidirectional_iterator<Iter>)
        {
            if (n < 0)
            {
                std::ranges::advance(begin_, n);
                if constexpr (StoreSize)
                    size_ += detail::to_unsigned_like(-n);
                return *this;
            }
        }

        auto d = n - std::ranges::advance(begin_, n, end_);
        if constexpr (StoreSize)
            size_ -= detail::to_unsigned_like(d);
        return *this;
    }

    constexpr friend bool operator==(borrowing_rad const & lhs, borrowing_rad const & rhs)
        requires detail::weakly_equality_comparable<std::iter_reference_t<Iter>>
    {
        return std::ranges::equal(lhs, rhs);
    }
};

template <typename TValue>
borrowing_rad(TValue *, TValue *) -> borrowing_rad<TValue *, TValue *, TValue const *, TValue const *>;

template <typename TValue>
borrowing_rad(TValue *, TValue *, std::make_unsigned_t<std::ptrdiff_t>)
  -> borrowing_rad<TValue *, TValue *, TValue const *, TValue const *, borrowing_rad_kind::sized>;

template <const_borrowable_range Range>
borrowing_rad(Range &&)
  -> borrowing_rad<std::ranges::iterator_t<Range>,
                   std::ranges::sentinel_t<Range>,
                   std::ranges::iterator_t<detail::add_const_t<Range>>,
                   std::ranges::sentinel_t<detail::add_const_t<Range>>,
                   (std::ranges::sized_range<Range> ? borrowing_rad_kind::sized : borrowing_rad_kind::unsized)>;

template <const_borrowable_range Range>
    requires(std::ranges::random_access_range<Range> && std::ranges::sized_range<Range>)
borrowing_rad(Range &&) -> borrowing_rad<std::ranges::iterator_t<Range>,
                                         std::ranges::iterator_t<Range>,
                                         std::ranges::iterator_t<detail::add_const_t<Range>>,
                                         std::ranges::iterator_t<detail::add_const_t<Range>>,
                                         borrowing_rad_kind::sized>;

template <const_borrowable_range Range>
    requires(std::ranges::contiguous_range<Range> && std::ranges::sized_range<Range>)
borrowing_rad(Range && r) -> borrowing_rad<decltype(std::to_address(std::ranges::begin(r))),
                                           decltype(std::to_address(std::ranges::begin(r))),
                                           decltype(std::to_address(std::ranges::cbegin(r))),
                                           decltype(std::to_address(std::ranges::cbegin(r))),
                                           borrowing_rad_kind::sized>;

template <const_borrowable_range Range>
borrowing_rad(Range &&, std::make_unsigned_t<std::ranges::range_difference_t<Range>>)
  -> borrowing_rad<std::ranges::iterator_t<Range>,
                   std::ranges::sentinel_t<Range>,
                   std::ranges::iterator_t<detail::add_const_t<Range>>,
                   std::ranges::sentinel_t<detail::add_const_t<Range>>,
                   borrowing_rad_kind::sized>;

template <const_borrowable_range Range>
    requires std::ranges::random_access_range<Range>
borrowing_rad(Range &&, std::make_unsigned_t<std::ranges::range_difference_t<Range>>)
  -> borrowing_rad<std::ranges::iterator_t<Range>,
                   std::ranges::iterator_t<Range>,
                   std::ranges::iterator_t<detail::add_const_t<Range>>,
                   std::ranges::iterator_t<detail::add_const_t<Range>>,
                   borrowing_rad_kind::sized>;

template <const_borrowable_range Range>
    requires(std::ranges::contiguous_range<Range>)
borrowing_rad(Range && r, std::make_unsigned_t<std::ranges::range_difference_t<Range>>)
  -> borrowing_rad<decltype(std::to_address(std::ranges::begin(r))),
                   decltype(std::to_address(std::ranges::begin(r))),
                   decltype(std::to_address(std::ranges::cbegin(r))),
                   decltype(std::to_address(std::ranges::cbegin(r))),
                   borrowing_rad_kind::sized>;

template <std::size_t Index, class Iter, class Sent, class CIter, class CSent, borrowing_rad_kind Kind>
    requires((Index == 0 && std::copyable<Iter>) || Index == 1)
constexpr auto get(borrowing_rad<Iter, Sent, CIter, CSent, Kind> const & borrowing_rad)
{
    if constexpr (Index == 0)
        return borrowing_rad.begin();
    else
        return borrowing_rad.end();
}

template <std::size_t Index, class Iter, class Sent, class CIter, class CSent, borrowing_rad_kind Kind>
    requires((Index == 0 && std::copyable<Iter>) || Index == 1)
constexpr auto get(borrowing_rad<Iter, Sent, CIter, CSent, Kind> & borrowing_rad)
{
    if constexpr (Index == 0)
        return borrowing_rad.begin();
    else
        return borrowing_rad.end();
}

template <std::size_t Index, class Iter, class Sent, class CIter, class CSent, borrowing_rad_kind Kind>
    requires(Index < 2)
constexpr auto get(borrowing_rad<Iter, Sent, CIter, CSent, Kind> && borrowing_rad)
{
    if constexpr (Index == 0)
        return borrowing_rad.begin();
    else
        return borrowing_rad.end();
}

} // namespace radr

namespace std
{

template <class Ip, class Sp, class CIp, class CSp, radr::borrowing_rad_kind Kp>
inline constexpr bool ranges::enable_borrowed_range<radr::borrowing_rad<Ip, Sp, CIp, CSp, Kp>> = true;

template <class Ip, class Sp, class CIp, class CSp, radr::borrowing_rad_kind Kp>
inline constexpr bool ranges::enable_view<radr::borrowing_rad<Ip, Sp, CIp, CSp, Kp>> = true;

using radr::get;

// [ranges.syn]

template <class Ip, class Sp, class CIp, class CSp, radr::borrowing_rad_kind Kp>
struct tuple_size<radr::borrowing_rad<Ip, Sp, CIp, CSp, Kp>> : integral_constant<size_t, 2>
{};

template <class Ip, class Sp, class CIp, class CSp, radr::borrowing_rad_kind Kp>
struct tuple_element<0, radr::borrowing_rad<Ip, Sp, CIp, CSp, Kp>>
{
    using type = Ip;
};

template <class Ip, class Sp, class CIp, class CSp, radr::borrowing_rad_kind Kp>
struct tuple_element<1, radr::borrowing_rad<Ip, Sp, CIp, CSp, Kp>>
{
    using type = Sp;
};

template <class Ip, class Sp, class CIp, class CSp, radr::borrowing_rad_kind Kp>
struct tuple_element<0, radr::borrowing_rad<Ip, Sp, CIp, CSp, Kp> const>
{
    using type = CIp;
};

template <class Ip, class Sp, class CIp, class CSp, radr::borrowing_rad_kind Kp>
struct tuple_element<1, radr::borrowing_rad<Ip, Sp, CIp, CSp, Kp> const>
{
    using type = CSp;
};

} // namespace std
