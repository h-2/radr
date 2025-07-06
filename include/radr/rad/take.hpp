// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023-2025 Hannes Hauswedell
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
#include "radr/concepts.hpp"

namespace radr::detail
{

template <std::forward_iterator Iter, std::sentinel_for<Iter> Sen>
class take_sentinel
{
    [[no_unique_address]] Sen end_{};

    template <std::forward_iterator Iter_, std::sentinel_for<Iter_> Sen_>
    friend class take_sentinel;

public:
    constexpr take_sentinel() = default;

    constexpr explicit take_sentinel(Sen end) : end_(std::move(end)) {}

    template <std::forward_iterator OtherIter, std::sentinel_for<OtherIter> OtherSen>
        requires(std::convertible_to<OtherIter, Iter> && std::convertible_to<OtherSen, Sen>)
    constexpr take_sentinel(take_sentinel<OtherIter, OtherSen> s) : end_(std::move(s.end_))
    {}

    constexpr Sen const & base() const & noexcept { return end_; }
    constexpr Sen         base() && noexcept { return std::move(end_); }

    // Clang complains about the following being constexpr for some reason
    friend bool operator==(take_sentinel const & lhs, take_sentinel const & rhs) = default;

    friend constexpr bool operator==(std::counted_iterator<Iter> const & lhs, take_sentinel const & rhs)
    {
        return lhs.count() == 0 || lhs.base() == rhs.end_;
    }
};

inline constexpr auto take_borrow =
  overloaded{[]<borrowed_mp_range URange>(URange && urange, range_size_t_or_size_t<URange> n)
{
    if constexpr (std::ranges::sized_range<URange>)
        n = std::min(n, std::ranges::size(urange));

    using ssize_t     = std::make_signed_t<range_size_t_or_size_t<URange>>;
    using ssize_t2    = std::conditional_t<(sizeof(ptrdiff_t) > sizeof(ssize_t)), ptrdiff_t, ssize_t>;
    ssize_t2 signed_n = static_cast<ssize_t2>(n);

    // this folds the type if the underlying iterator is already std::counted_iterator
    auto make_counted_iterator = overloaded{[](auto uit, ssize_t2 s) {
        return std::counted_iterator{std::move(uit), s};
    },
                                            []<typename UUIt>(std::counted_iterator<UUIt> uit, ssize_t2 s)
    {
        return std::counted_iterator{std::move(uit).base(), std::min(uit.count(), s)};
    }};

    using it_t  = decltype(make_counted_iterator(radr::begin(urange), signed_n));
    using cit_t = decltype(make_counted_iterator(radr::cbegin(urange), signed_n));

    if constexpr (std::ranges::sized_range<URange>)
    {
        using BorrowingRad =
          borrowing_rad<it_t, std::default_sentinel_t, cit_t, std::default_sentinel_t, borrowing_rad_kind::sized>;

        return BorrowingRad{make_counted_iterator(radr::begin(urange), signed_n), std::default_sentinel, n};
    }
    else
    {
        // this folds the type if the underlying sentinel is already take_sentinel
        auto make_sentinel =
          overloaded{[]<typename UIt, typename USen>(UIt, USen usen) { return take_sentinel<UIt, USen>{usen}; },
                     []<typename UIt, typename UUIt, typename UUSen>(UIt, take_sentinel<UUIt, UUSen> usen)
        {
            return take_sentinel<UUIt, UUSen>{std::move(usen).base()};
        }};

        using sen_t  = decltype(make_sentinel(radr::begin(urange), radr::end(urange)));
        using csen_t = decltype(make_sentinel(radr::cbegin(urange), radr::cend(urange)));

        using BorrowingRad = borrowing_rad<it_t, sen_t, cit_t, csen_t, borrowing_rad_kind::unsized>;

        return BorrowingRad{make_counted_iterator(radr::begin(urange), signed_n),
                            make_sentinel(radr::begin(urange), radr::end(urange))};
    }
},
             []<borrowed_mp_range URange>(URange && urange, range_size_t_or_size_t<URange> n)
                 requires(safely_indexable_range<URange>)
{
    return subborrow(std::forward<URange>(urange), 0ull, n);
}};

inline constexpr auto take_coro = []<std::ranges::input_range URange>(URange && urange, std::size_t const n)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    // we need to create inner functor so that it can take by value
    return [](auto urange_, std::size_t const n)
             -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        size_t i  = 0;
        auto   it = radr::begin(urange_);
        if (i == n || it == radr::end(urange_))
            co_return;

        while (true)
        {
            co_yield *it;

            ++i;
            if (i == n)
                break;

            /* checks are split up to prevent iterating further than necessary */
            ++it;
            if (it == radr::end(urange_))
                break;
        }
    }(std::move(urange), n);
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Take up to n elements from the underlying range.
 * \param[in] urange The underlying range.
 * \param[in] n Number of elements.
 * \tparam URange Type of \p urange.
 * \details
 *
 * Takes \p n elements from \p urange, or all elements if \p urange is smaller than \p n.
 *
 * ## Multi-pass ranges
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *
 * For underlying ranges that are radr::safely_indexable_range, this adaptor invokes the radr::subborrow
 * customisation point.
 * Unless customised otherwise and if the range was sized, it will preserve all range concepts and be
 * transparent (same iterator and senintel types as \p urange).
 * If it was infinite, it will become sized and common.
 *
 * For underlying ranges that are not radr::safely_indexable_range, this adaptors preserves:
 *   * categories up to std::ranges::contiguous_range
 *   * std::ranges::borrowed_range
 *   * radr::mutable_range
 *   * radr::constant_range
 *   * std::ranges::sized_range
 *
 * It does not preserve:
 *   * radr::common_range
 *
 * ### Notable differences to std::views::take
 *
 *   * Subrange customisation through radr::subborrow.
 *   * Returns sized, common ranges for infinite inputs like radr::iota and radr::repeat.
 *
 * ## Single-pass ranges
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *
 * ### Notable difference std::views::take
 *
 * std::views::take takes \p n elements from the underlying range but then silently advances over the `n+1`-th element,
 * resulting in unexpected behaviour (e.g. skipped/missing elements in a stream).
 * See https://www.youtube.com/watch?v=dvi0cl8ccNQ for an explenation.
 *
 * We **fixed this bug** for radr::take on single-pass ranges.
 */
inline constexpr auto take = detail::pipe_with_args_fn{detail::take_coro, detail::take_borrow};
} // namespace cpo
} // namespace radr
