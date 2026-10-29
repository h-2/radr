// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023-2026 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

#include "radr/class/borrowing_rad.hpp"
#include "radr/detail/detail.hpp"
#include "radr/detail/fwd.hpp"
#include "radr/detail/pipe.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{

/*!\brief Sentinel for std::counted_iterator that denotes the end of a chunk.
 * \tparam Diff The difference type.
 * \tparam K The chunk size; the iterator starts with count K.
 * \tparam Bounded Whether to additionally test for count 0.
 * \details
 *
 * The chunk ends when K minus the count reaches the chunk's length. Comparing against the length (and not against
 * K minus the length) lets the compiler fold the computation of the length into the comparison.
 *
 * Testing for count 0 is redundant, but gives the compiler a constant bound (K) for loops over the chunk. This is
 * needed by clang for unsized underlying ranges. For sized underlying ranges, the bound is known anyway, and the
 * additional test prevents GCC from unrolling loops over e.g. std::deque.
 */
template <std::integral Diff, Diff K, bool Bounded>
struct chunk_counted_sentinel
{
    Diff len{};

    template <typename I>
    friend constexpr bool operator==(std::counted_iterator<I> const & it, chunk_counted_sentinel const & s)
    {
        if constexpr (Bounded)
            return it.count() == 0 || K - it.count() == s.len;
        else
            return K - it.count() >= s.len;
    }

    template <typename I>
    friend constexpr std::iter_difference_t<I> operator-(chunk_counted_sentinel const &   s,
                                                         std::counted_iterator<I> const & it)
    {
        return s.len - (K - it.count());
    }

    template <typename I>
    friend constexpr std::iter_difference_t<I> operator-(std::counted_iterator<I> const & it,
                                                         chunk_counted_sentinel const &   s)
    {
        return (K - it.count()) - s.len;
    }
};

//!\brief Inner range of radr::chunk_counted.
template <borrowed_mp_range Borrow, ptrdiff_t K, bool Bounded>
using chunk_counted_inner_t =
  borrowing_rad<std::counted_iterator<iterator_t<Borrow>>,
                chunk_counted_sentinel<std::ranges::range_difference_t<Borrow>, K, Bounded>,
                std::counted_iterator<const_iterator_t<Borrow>>,
                chunk_counted_sentinel<std::ranges::range_difference_t<Borrow>, K, Bounded>>;

/*!\brief Create the inner range of radr::chunk_counted from the chunk's begin and length.
 * \details
 * In contrast to radr::slide_counted, the inner range is never created via radr::subborrow (not even for contiguous
 * ranges), because the length of the last chunk is not known at compile-time. A loop over a subborrowed chunk has
 * a run-time trip count, and GCC and clang were not observed to unroll it.
 */
template <borrowed_mp_range Borrow, ptrdiff_t K, bool Bounded>
constexpr chunk_counted_inner_t<Borrow, K, Bounded> chunk_counted_make_inner(
  iterator_t<Borrow> const                      b,
  std::ranges::range_difference_t<Borrow> const len)
{
    using Diff = std::ranges::range_difference_t<Borrow>;
    return {
      std::counted_iterator{b, static_cast<Diff>(K)},
      chunk_counted_sentinel<Diff, K, Bounded>{len}
    };
}

/*!\brief The iterator of radr::chunk_counted for sized underlying ranges.
 * \tparam Borrow The (borrowed) underlying range.
 * \tparam K The chunk size.
 * \details
 *
 * Stores the underlying size and the offset of the current chunk (always a multiple of K; the end is the number of
 * chunks times K). If the underlying range is contiguous, the underlying begin is stored and the chunk's begin
 * is computed on dereference; otherwise, the chunk's begin is stored.
 */
template <borrowed_mp_range Borrow, ptrdiff_t K>
    requires std::ranges::sized_range<Borrow>
class chunk_counted_sized_iterator
{
private:
    using UIt  = iterator_t<Borrow>;
    using Diff = std::ranges::range_difference_t<Borrow>;

    static constexpr bool ra           = std::ranges::random_access_range<Borrow>;
    static constexpr bool store_ubegin = std::contiguous_iterator<UIt>;

    [[no_unique_address]] UIt  xbegin{}; // ubegin if store_ubegin; subrange_begin otherwise
    [[no_unique_address]] Diff usize{};  // underlying size
    [[no_unique_address]] Diff i{};      // offset of the chunk (multiple of K)

    template <borrowed_mp_range Borrow2, ptrdiff_t K2>
        requires std::ranges::sized_range<Borrow2>
    friend class chunk_counted_sized_iterator;

    template <typename Container>
    constexpr friend chunk_counted_sized_iterator tag_invoke(custom::rebind_iterator_tag,
                                                             chunk_counted_sized_iterator it,
                                                             Container &                  container_old,
                                                             Container &                  container_new)
    {
        it.xbegin = tag_invoke(custom::rebind_iterator_tag{}, it.xbegin, container_old, container_new);
        return it;
    }

    //!\brief Length of the chunk at offset i_; unsigned, so that the compiler knows the result is in [0, K].
    constexpr Diff len(Diff const i_) const
    {
        return static_cast<Diff>(std::min(static_cast<size_t>(K), static_cast<size_t>(usize - i_)));
    }

public:
    /*!\name Associated types
     * \{
     */
    using iterator_concept  = std::conditional_t<ra,
                                                 std::random_access_iterator_tag,
                                                 std::conditional_t<std::ranges::bidirectional_range<Borrow>,
                                                                    std::bidirectional_iterator_tag,
                                                                    std::forward_iterator_tag>>;
    using iterator_category = std::input_iterator_tag;
    using value_type        = chunk_counted_inner_t<Borrow, K, false>;
    using difference_type   = Diff;
    //!\}

    /*!\name Constructors, destructor and assignments.
     * \{
     */
    constexpr chunk_counted_sized_iterator()                                                 = default;
    constexpr chunk_counted_sized_iterator(chunk_counted_sized_iterator const &)             = default;
    constexpr chunk_counted_sized_iterator(chunk_counted_sized_iterator &&)                  = default;
    constexpr chunk_counted_sized_iterator & operator=(chunk_counted_sized_iterator const &) = default;
    constexpr chunk_counted_sized_iterator & operator=(chunk_counted_sized_iterator &&)      = default;

    //!\brief Construct at the beginning.
    constexpr chunk_counted_sized_iterator(Borrow urange_) :
      xbegin{radr::begin(urange_)}, usize{static_cast<Diff>(std::ranges::size(urange_))}
    {}

    //!\brief Construct at the end (random-access or common ranges only).
    constexpr chunk_counted_sized_iterator(Borrow urange_, std::default_sentinel_t)
        requires(ra || common_range<Borrow>)
      : chunk_counted_sized_iterator{urange_}
    {
        i = (usize + K - 1) / K * K;
        if constexpr (!store_ubegin && common_range<Borrow>)
            xbegin = radr::end(urange_);
        else if constexpr (!store_ubegin)
            xbegin += usize;
    }

    //!\brief Construct from compatible iterator, in particular non-const to const.
    template <different_from<Borrow> Borrow2>
        requires std::constructible_from<UIt, typename chunk_counted_sized_iterator<Borrow2, K>::UIt>
    constexpr chunk_counted_sized_iterator(chunk_counted_sized_iterator<Borrow2, K> mut_iter) :
      xbegin{std::move(mut_iter.xbegin)}, usize{mut_iter.usize}, i{mut_iter.i}
    {}
    //!\}

    /*!\name Iterator operators
     * \{
     */
    constexpr value_type operator*() const
    {
        if constexpr (store_ubegin)
            return chunk_counted_make_inner<Borrow, K, false>(xbegin + i, len(i));
        else
            return chunk_counted_make_inner<Borrow, K, false>(xbegin, len(i));
    }

    constexpr chunk_counted_sized_iterator & operator++()
    {
        if constexpr (!store_ubegin)
            std::ranges::advance(xbegin, len(i));
        i += K;
        return *this;
    }

    constexpr chunk_counted_sized_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr chunk_counted_sized_iterator & operator--()
        requires std::ranges::bidirectional_range<Borrow>
    {
        i -= K;
        if constexpr (!store_ubegin)
            std::ranges::advance(xbegin, -len(i));
        return *this;
    }

    constexpr chunk_counted_sized_iterator operator--(int)
        requires std::ranges::bidirectional_range<Borrow>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr chunk_counted_sized_iterator & operator+=(difference_type k)
        requires ra
    {
        if constexpr (!store_ubegin)
            xbegin += std::min(i + k * K, usize) - std::min(i, usize);
        i += k * K;
        return *this;
    }

    constexpr chunk_counted_sized_iterator & operator-=(difference_type k)
        requires ra
    {
        return *this += -k;
    }

    constexpr value_type operator[](difference_type k) const
        requires ra
    {
        return *(*this + k);
    }

    friend constexpr chunk_counted_sized_iterator operator+(chunk_counted_sized_iterator it, difference_type k)
        requires ra
    {
        it += k;
        return it;
    }

    friend constexpr chunk_counted_sized_iterator operator+(difference_type k, chunk_counted_sized_iterator it)
        requires ra
    {
        it += k;
        return it;
    }

    friend constexpr chunk_counted_sized_iterator operator-(chunk_counted_sized_iterator it, difference_type k)
        requires ra
    {
        it -= k;
        return it;
    }

    friend constexpr difference_type operator-(chunk_counted_sized_iterator const & x,
                                               chunk_counted_sized_iterator const & y)
    {
        return (x.i - y.i) / K;
    }

    friend constexpr difference_type operator-(std::default_sentinel_t, chunk_counted_sized_iterator const & x)
    {
        return (x.usize + K - 1) / K - x.i / K;
    }

    friend constexpr difference_type operator-(chunk_counted_sized_iterator const & x, std::default_sentinel_t)
    {
        return x.i / K - (x.usize + K - 1) / K;
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    friend constexpr bool operator==(chunk_counted_sized_iterator const & x, chunk_counted_sized_iterator const & y)
    {
        return x.i == y.i;
    }

    friend constexpr bool operator==(chunk_counted_sized_iterator const & x, std::default_sentinel_t)
    {
        return x.i >= x.usize;
    }

    friend constexpr auto operator<=>(chunk_counted_sized_iterator const & x, chunk_counted_sized_iterator const & y)
    {
        return x.i <=> y.i;
    }
    //!\}
};

/*!\brief The forward iterator of radr::chunk_counted for unsized underlying ranges.
 * \tparam Borrow The (borrowed) underlying range.
 * \tparam K The chunk size.
 */
template <borrowed_mp_range Borrow, ptrdiff_t K>
class chunk_counted_unsized_iterator
{
private:
    using UIt  = iterator_t<Borrow>;
    using USen = sentinel_t<Borrow>;
    using Diff = std::ranges::range_difference_t<Borrow>;

    [[no_unique_address]] USen uend{};
    [[no_unique_address]] UIt  subrange_begin{};
    [[no_unique_address]] UIt  subrange_end{};
    [[no_unique_address]] Diff len{};

    template <borrowed_mp_range Borrow2, ptrdiff_t K2>
    friend class chunk_counted_unsized_iterator;

    template <typename Container>
    constexpr friend chunk_counted_unsized_iterator tag_invoke(custom::rebind_iterator_tag,
                                                               chunk_counted_unsized_iterator it,
                                                               Container &                    container_old,
                                                               Container &                    container_new)
    {
        it.uend           = tag_invoke(custom::rebind_iterator_tag{}, it.uend, container_old, container_new);
        it.subrange_begin = tag_invoke(custom::rebind_iterator_tag{}, it.subrange_begin, container_old, container_new);
        it.subrange_end   = tag_invoke(custom::rebind_iterator_tag{}, it.subrange_end, container_old, container_new);
        return it;
    }

    //!\brief Advance subrange_end by up to K elements.
    constexpr void find_end()
    {
        // not std::ranges::next(it, n, bound), because GCC does not unroll that here
        for (len = 0; len < K && subrange_end != uend; ++len)
            ++subrange_end;
    }

public:
    /*!\name Associated types
     * \{
     */
    using iterator_concept  = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type        = chunk_counted_inner_t<Borrow, K, true>;
    using difference_type   = Diff;
    //!\}

    /*!\name Constructors, destructor and assignments.
     * \{
     */
    constexpr chunk_counted_unsized_iterator()                                                   = default;
    constexpr chunk_counted_unsized_iterator(chunk_counted_unsized_iterator const &)             = default;
    constexpr chunk_counted_unsized_iterator(chunk_counted_unsized_iterator &&)                  = default;
    constexpr chunk_counted_unsized_iterator & operator=(chunk_counted_unsized_iterator const &) = default;
    constexpr chunk_counted_unsized_iterator & operator=(chunk_counted_unsized_iterator &&)      = default;

    //!\brief Construct at the beginning; O(K).
    constexpr chunk_counted_unsized_iterator(Borrow urange_) :
      uend{radr::end(urange_)}, subrange_begin{radr::begin(urange_)}, subrange_end{subrange_begin}
    {
        find_end();
    }

    //!\brief Construct at the end (common ranges only).
    constexpr chunk_counted_unsized_iterator(Borrow urange_, std::default_sentinel_t)
        requires common_range<Borrow>
      : uend{radr::end(urange_)}, subrange_begin{uend}, subrange_end{uend}
    {}

    //!\brief Construct from compatible iterator, in particular non-const to const.
    template <different_from<Borrow> Borrow2>
        requires(std::constructible_from<UIt, typename chunk_counted_unsized_iterator<Borrow2, K>::UIt> &&
                 std::constructible_from<USen, typename chunk_counted_unsized_iterator<Borrow2, K>::USen>)
    constexpr chunk_counted_unsized_iterator(chunk_counted_unsized_iterator<Borrow2, K> mut_iter) :
      uend{std::move(mut_iter.uend)},
      subrange_begin{std::move(mut_iter.subrange_begin)},
      subrange_end{std::move(mut_iter.subrange_end)},
      len{mut_iter.len}
    {}
    //!\}

    /*!\name Iterator operators
     * \{
     */
    constexpr value_type operator*() const { return chunk_counted_make_inner<Borrow, K, true>(subrange_begin, len); }

    constexpr chunk_counted_unsized_iterator & operator++()
    {
        subrange_begin = subrange_end;
        find_end();
        return *this;
    }

    constexpr chunk_counted_unsized_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    friend constexpr bool operator==(chunk_counted_unsized_iterator const & x, chunk_counted_unsized_iterator const & y)
    {
        return x.subrange_begin == y.subrange_begin;
    }

    //!\brief Chunks are never empty, so an empty current chunk denotes the end.
    friend constexpr bool operator==(chunk_counted_unsized_iterator const & x, std::default_sentinel_t)
    {
        return x.len == 0;
    }
    //!\}
};

} // namespace radr::detail

#if RADR_BUG_GCC_CONCEPT_RECURSION
template <typename Borrow, ptrdiff_t K>
struct std::iterator_traits<radr::detail::chunk_counted_sized_iterator<Borrow, K>>
{
    using iterator_concept  = typename radr::detail::chunk_counted_sized_iterator<Borrow, K>::iterator_concept;
    using iterator_category = std::input_iterator_tag;
    using value_type        = typename radr::detail::chunk_counted_sized_iterator<Borrow, K>::value_type;
    using difference_type   = typename radr::detail::chunk_counted_sized_iterator<Borrow, K>::difference_type;
    using pointer           = void;
    using reference         = value_type;
};

template <typename Borrow, ptrdiff_t K>
struct std::iterator_traits<radr::detail::chunk_counted_unsized_iterator<Borrow, K>>
{
    using iterator_concept  = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type        = typename radr::detail::chunk_counted_unsized_iterator<Borrow, K>::value_type;
    using difference_type   = typename radr::detail::chunk_counted_unsized_iterator<Borrow, K>::difference_type;
    using pointer           = void;
    using reference         = value_type;
};
#endif

namespace radr::detail
{

template <ptrdiff_t K>
inline constexpr auto chunk_counted_borrow = []<borrowed_mp_range URange>(URange && urange)
{
    static_assert(K > 0, "You must select K > 0 for radr::chunk_counted.");

    auto borrow_ = radr::borrow(urange);

    if constexpr (std::ranges::sized_range<URange>)
    {
        using It  = chunk_counted_sized_iterator<decltype(borrow_), K>;
        using CIt = chunk_counted_sized_iterator<borrow_t<std::remove_cvref_t<URange> const &>, K>;

        if constexpr (std::ranges::random_access_range<URange> || common_range<URange>)
            return borrowing_rad<It, It, CIt, CIt>{
              It{borrow_},
              It{borrow_, std::default_sentinel}
            };
        else
            return borrowing_rad<It, std::default_sentinel_t, CIt, std::default_sentinel_t>{It{borrow_},
                                                                                            std::default_sentinel};
    }
    else
    {
        using It  = chunk_counted_unsized_iterator<decltype(borrow_), K>;
        using CIt = chunk_counted_unsized_iterator<borrow_t<std::remove_cvref_t<URange> const &>, K>;

        if constexpr (common_range<URange>)
        {
            return borrowing_rad<It, It, CIt, CIt>{
              It{borrow_},
              It{borrow_, std::default_sentinel}
            };
        }
        else
        {
            using Sen =
              std::conditional_t<infinite_mp_range<URange>, std::unreachable_sentinel_t, std::default_sentinel_t>;
            return borrowing_rad<It, Sen, CIt, Sen>{It{borrow_}, Sen{}};
        }
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{

/*!\brief radr::chunk_counted<K>(urange)
 * \tparam K The size of the chunks (must be > 0).
 * \param urange The underlying range.
 * \details
 *
 * Turns a range into a range-of-ranges, by chunking it into consecutive, non-overlapping
 * subranges of size \p K each, except possibly the last one, which is shorter if the size
 * of \p urange is not a multiple of \p K.
 *
 * The produced elements are identical to those of radr::chunk, but the types are weaker. On the other hand,
 * this adaptor can be faster than radr::chunk (and std::views::chunk), because the compiler can reliably unroll loops
 * over a chunk.
 *
 * |                           |    radr::chunk                    | radr::chunk_counted        |
 * |---------------------------|-----------------------------------|----------------------------|
 * | chunk size                | runtime parameter                 | compile-time parameter     |
 * | inner range               | always created by radr::subborrow | uses std::counted_iterator |
 * | iterator size             | larger                            | smaller or equal           |
 *
 * In general, prefer radr::chunk for the better types. Use radr::chunk_counted only if the chunk size is known at
 * compile-time and small, *and* if you are iterating over the chunk in a tight loop.
 *
 * ## Multi-pass ranges
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *
 * The returned "outer range"-type preserves from the underlying range:
 *   * std::ranges::random_access_range (only if also sized)
 *   * std::ranges::bidirectional_range (only if also sized)
 *   * std::ranges::sized_range
 *   * radr::common_range (also if random-access and sized)
 *   * radr::mutable_range
 *   * radr::constant_range
 *   * radr::infinite_mp_range
 *
 * The returned "inner range" is a pair of std::counted_iterator (that always starts with count \p K) and a sentinel
 * that compares the number of elements visited against the chunk's length. It always models
 * std::ranges::borrowed_range and std::ranges::sized_range, but radr::common_range only if the underlying range is
 * random-access (via radr::end).
 *
 * It preserves from the underlying range:
 *   * categories up to std::ranges::contiguous_range
 *   * radr::mutable_range
 *   * radr::constant_range
 *
 * Construction of the adaptor is in O(1) for sized underlying ranges and in O(K) otherwise.
 *
 * ### Performance
 *
 * On random access ranges, it is considerably faster than std::views::chunk and radr::chunk when iterating over
 * the elements of each chunk. On latency-bound ranges like std::list, all of them perform identically.
 *
 * ## Single-pass ranges
 *
 * radr::chunk_counted cannot be created on single-pass ranges.
 */
template <ptrdiff_t K>
inline constexpr auto chunk_counted = detail::pipe_without_args_fn<void, decltype(detail::chunk_counted_borrow<K>)>{};

} // namespace cpo
} // namespace radr
