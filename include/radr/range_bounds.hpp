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

#include "detail.hpp"
#include "fwd.hpp"
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

} // namespace radr::detail

namespace radr
{

template <std::input_or_output_iterator Iter,
          std::sentinel_for<Iter>       Sent = Iter,
          typename CIter                     = std::nullptr_t,
          typename CSent                     = std::nullptr_t,
          range_bounds_kind Kind =
            std::sized_sentinel_for<Sent, Iter> ? range_bounds_kind::sized : range_bounds_kind::unsized>
    requires(Kind == range_bounds_kind::sized || !std::sized_sentinel_for<Sent, Iter>)
class range_bounds : public rad_interface<range_bounds<Iter, Sent, CIter, CSent, Kind>>
{
public:
    // Note: this is an internal implementation detail that is public only for internal usage.
    static constexpr bool StoreSize = (Kind == range_bounds_kind::sized && !std::sized_sentinel_for<Sent, Iter>);

private:
    static constexpr bool MustProvideSizeAtConstruction = !StoreSize; // just to improve compiler diagnostics
    struct Empty
    {
        constexpr Empty(auto) noexcept {}
    };
    using Size = std::conditional_t<StoreSize, std::make_unsigned_t<std::iter_difference_t<Iter>>, Empty>;

    static constexpr bool const_iterable = !std::same_as<CIter, std::nullptr_t> || !std::same_as<CSent, std::nullptr_t>;

    static constexpr bool simple = std::same_as<CIter, Iter> && std::same_as<CSent, Sent>;

    static_assert(!const_iterable || std::sentinel_for<CSent, CIter>,
                  "The const_sentinel is not a sentinel for the const_iterator.");
    static_assert(!const_iterable || std::convertible_to<Iter, CIter>,
                  "The iterator is not convertible to the const_iterator.");
    static_assert(!const_iterable || std::convertible_to<Sent, CSent>,
                  "The sentinel is not convertible to the const_sentinel.");

    [[no_unique_address]] Iter begin_ = Iter();
    [[no_unique_address]] Sent end_   = Sent();
    [[no_unique_address]] Size size_  = 0;

public:
    range_bounds()
        requires std::default_initializable<Iter>
    = default;

    constexpr range_bounds(detail::convertible_to_non_slicing<Iter> auto iter, Sent sent)
        requires MustProvideSizeAtConstruction
      : begin_(std::move(iter)), end_(std::move(sent))
    {}

    constexpr range_bounds(detail::convertible_to_non_slicing<Iter> auto      iter,
                           Sent                                               sent,
                           std::make_unsigned_t<std::iter_difference_t<Iter>> n)
        requires(Kind == range_bounds_kind::sized)
      : begin_(std::move(iter)), end_(std::move(sent)), size_(n)
    {
        if constexpr (std::sized_sentinel_for<Sent, Iter>)
            assert((end_ - begin_) == static_cast<std::iter_difference_t<Iter>>(n));
    }

    template <detail::different_from<range_bounds> Range>
        requires(std::ranges::borrowed_range<Range> &&
                 detail::convertible_to_non_slicing<std::ranges::iterator_t<Range>, Iter> &&
                 std::convertible_to<std::ranges::sentinel_t<Range>, Sent>)
    constexpr range_bounds(Range && range)
        requires(!StoreSize)
      : range_bounds(std::ranges::begin(range), std::ranges::end(range))
    {}

    template <detail::different_from<range_bounds> Range>
        requires(std::ranges::borrowed_range<Range> &&
                 detail::convertible_to_non_slicing<std::ranges::iterator_t<Range>, Iter> &&
                 std::convertible_to<std::ranges::sentinel_t<Range>, Sent>)
    constexpr range_bounds(Range && range)
        requires StoreSize && std::ranges::sized_range<Range>
      : range_bounds(range, std::ranges::size(range))
    {}

    template <std::ranges::borrowed_range Range>
        requires(detail::convertible_to_non_slicing<std::ranges::iterator_t<Range>, Iter> &&
                 std::convertible_to<std::ranges::sentinel_t<Range>, Sent>)
    constexpr range_bounds(Range && range, std::make_unsigned_t<std::iter_difference_t<Iter>> n)
        requires(Kind == range_bounds_kind::sized)
      : range_bounds(std::ranges::begin(range), std::ranges::end(range), n)
    {}

    template <detail::different_from<range_bounds> Pair>
        requires detail::pair_like_convertible_from<Pair, Iter const &, Sent const &>
    constexpr operator Pair() const
    {
        return Pair(begin_, end_);
    }

    constexpr Iter begin()
        requires(std::copyable<Iter> && !simple)
    {
        return begin_;
    }

    constexpr Iter begin() const
        requires(std::copyable<Iter> && const_iterable && simple)
    {
        return begin_;
    }

    constexpr auto begin() const
        requires(std::copyable<Iter> && const_iterable && !simple)
    {
        return static_cast<CIter>(begin_);
    }

    // Only single-pass input ranges can have move-only iterators and these are not const-iterable
    [[nodiscard]] constexpr Iter begin()
        requires(!std::copyable<Iter>)
    {
        return std::move(begin_);
    }

    constexpr Sent end()
        requires(!simple)
    {
        return end_;
    }

    constexpr Sent end() const
        requires(const_iterable && simple)
    {
        return end_;
    }

    constexpr auto end() const
        requires(const_iterable && !simple)
    {
        return static_cast<CSent>(end_);
    }

    [[nodiscard]] constexpr bool empty() const { return begin_ == end_; }

    constexpr std::make_unsigned_t<std::iter_difference_t<Iter>> size() const
        requires(Kind == range_bounds_kind::sized)
    {
        if constexpr (StoreSize)
            return size_;
        else
            return detail::to_unsigned_like(end_ - begin_);
    }

    [[nodiscard]] constexpr range_bounds next(std::iter_difference_t<Iter> n = 1) const &
        requires std::forward_iterator<Iter>
    {
        auto tmp = *this;
        tmp.advance(n);
        return tmp;
    }

    [[nodiscard]] constexpr range_bounds next(std::iter_difference_t<Iter> n = 1) &&
    {
        advance(n);
        return std::move(*this);
    }

    [[nodiscard]] constexpr range_bounds prev(std::iter_difference_t<Iter> n = 1) const
        requires std::bidirectional_iterator<Iter>
    {
        auto tmp = *this;
        tmp.advance(-n);
        return tmp;
    }

    constexpr range_bounds & advance(std::iter_difference_t<Iter> n)
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
};

template <std::input_or_output_iterator Iter, std::sentinel_for<Iter> Sent>
range_bounds(Iter, Sent) -> range_bounds<Iter, Sent>;

template <typename TValue>
range_bounds(TValue *, TValue *) -> range_bounds<TValue *, TValue *, TValue const *, TValue const *>;

template <std::input_or_output_iterator Iter, std::sentinel_for<Iter> Sent>
range_bounds(Iter, Sent, std::make_unsigned_t<std::iter_difference_t<Iter>>)
  -> range_bounds<Iter, Sent, std::nullptr_t, std::nullptr_t, range_bounds_kind::sized>;

template <typename TValue>
range_bounds(TValue *, TValue *, std::make_unsigned_t<std::ptrdiff_t>)
  -> range_bounds<TValue *, TValue *, TValue const *, TValue const *, range_bounds_kind::sized>;

template <std::ranges::borrowed_range Range>
range_bounds(Range &&)
  -> range_bounds<std::ranges::iterator_t<Range>,
                  std::ranges::sentinel_t<Range>,
                  std::nullptr_t,
                  std::nullptr_t,
                  (std::ranges::sized_range<Range> ||
                   std::sized_sentinel_for<std::ranges::sentinel_t<Range>, std::ranges::iterator_t<Range>>)
                    ? range_bounds_kind::sized
                    : range_bounds_kind::unsized>;

template <std::ranges::borrowed_range Range>
    requires std::ranges::range<std::remove_cvref_t<Range> const &>
range_bounds(Range &&)
  -> range_bounds<std::ranges::iterator_t<Range>,
                  std::ranges::sentinel_t<Range>,
                  std::ranges::iterator_t<std::remove_cvref_t<Range> const &>,
                  std::ranges::sentinel_t<std::remove_cvref_t<Range> const &>,
                  (std::ranges::sized_range<Range> ||
                   std::sized_sentinel_for<std::ranges::sentinel_t<Range>, std::ranges::iterator_t<Range>>)
                    ? range_bounds_kind::sized
                    : range_bounds_kind::unsized>;

template <std::ranges::borrowed_range Range>
range_bounds(Range &&, std::make_unsigned_t<std::ranges::range_difference_t<Range>>)
  -> range_bounds<std::ranges::iterator_t<Range>,
                  std::ranges::sentinel_t<Range>,
                  std::nullptr_t,
                  std::nullptr_t,
                  range_bounds_kind::sized>;

template <std::ranges::borrowed_range Range>
    requires std::ranges::range<std::remove_cvref_t<Range> const &>
range_bounds(Range &&, std::make_unsigned_t<std::ranges::range_difference_t<Range>>)
  -> range_bounds<std::ranges::iterator_t<Range>,
                  std::ranges::sentinel_t<Range>,
                  std::ranges::iterator_t<std::remove_cvref_t<Range> const &>,
                  std::ranges::sentinel_t<std::remove_cvref_t<Range> const &>,
                  range_bounds_kind::sized>;

template <std::size_t Index, class Iter, class Sent, class CIter, class CSent, range_bounds_kind Kind>
    requires((Index == 0 && std::copyable<Iter>) || Index == 1)
constexpr auto get(range_bounds<Iter, Sent, CIter, CSent, Kind> const & range_bounds)
{
    if constexpr (Index == 0)
        return range_bounds.begin();
    else
        return range_bounds.end();
}

template <std::size_t Index, class Iter, class Sent, class CIter, class CSent, range_bounds_kind Kind>
    requires((Index == 0 && std::copyable<Iter>) || Index == 1)
constexpr auto get(range_bounds<Iter, Sent, CIter, CSent, Kind> & range_bounds)
{
    if constexpr (Index == 0)
        return range_bounds.begin();
    else
        return range_bounds.end();
}

template <std::size_t Index, class Iter, class Sent, class CIter, class CSent, range_bounds_kind Kind>
    requires(Index < 2)
constexpr auto get(range_bounds<Iter, Sent, CIter, CSent, Kind> && range_bounds)
{
    if constexpr (Index == 0)
        return range_bounds.begin();
    else
        return range_bounds.end();
}

} // namespace radr

namespace std
{

template <class Ip, class Sp, class CIp, class CSp, radr::range_bounds_kind Kp>
inline constexpr bool ranges::enable_borrowed_range<radr::range_bounds<Ip, Sp, CIp, CSp, Kp>> = true;

template <class Ip, class Sp, class CIp, class CSp, radr::range_bounds_kind Kp>
inline constexpr bool ranges::enable_view<radr::range_bounds<Ip, Sp, CIp, CSp, Kp>> = true;

// template <std::ranges::range Rp>
// using std::borrowed_range_bounds_t =
//   If<std::ranges::borrowed_range<Rp>, range_bounds<std::ranges::iterator_t<Rp>>, dangling>;

using radr::get;

// [ranges.syn]

template <class Ip, class Sp, class CIp, class CSp, radr::range_bounds_kind Kp>
struct tuple_size<radr::range_bounds<Ip, Sp, CIp, CSp, Kp>> : integral_constant<size_t, 2>
{};

template <class Ip, class Sp, class CIp, class CSp, radr::range_bounds_kind Kp>
struct tuple_element<0, radr::range_bounds<Ip, Sp, CIp, CSp, Kp>>
{
    using type = Ip;
};

template <class Ip, class Sp, class CIp, class CSp, radr::range_bounds_kind Kp>
struct tuple_element<1, radr::range_bounds<Ip, Sp, CIp, CSp, Kp>>
{
    using type = Sp;
};

template <class Ip, class Sp, class CIp, class CSp, radr::range_bounds_kind Kp>
struct tuple_element<0, const radr::range_bounds<Ip, Sp, CIp, CSp, Kp>>
{
    using type = CIp;
};

template <class Ip, class Sp, class CIp, class CSp, radr::range_bounds_kind Kp>
struct tuple_element<1, const radr::range_bounds<Ip, Sp, CIp, CSp, Kp>>
{
    using type = CSp;
};

} // namespace std
