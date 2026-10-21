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

#include <cassert>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

#include "radr/class/borrowing_rad.hpp"
#include "radr/custom/subborrow.hpp"
#include "radr/detail/detail.hpp"
#include "radr/detail/fwd.hpp"
#include "radr/detail/pipe.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{

/*!\brief The iterator of radr::slide_counted.
 * \tparam Borrow The (borrowed) underlying range.
 * \tparam K The window size.
 * \details
 *
 * If the underlying range is bidirectional+common, only the begin of the window is stored and the end iterator
 * is created once, at the begin of the (non-existent) window after the last one; iterators compare their begins.
 * Otherwise, the last element of the window (inclusive) is stored and compared; the underlying end is only stored
 * if the underlying range is not common.
 */
template <borrowed_mp_range Borrow, ptrdiff_t K>
class slide_counted_iterator
{
private:
    using UIt  = iterator_t<Borrow>;
    using USen = sentinel_t<Borrow>;
    using UCIt = const_iterator_t<Borrow>;

    static constexpr bool bidi_common = std::ranges::bidirectional_range<Borrow> && common_range<Borrow>;
    static constexpr bool store_last  = !bidi_common;
    static constexpr bool store_end   = !common_range<Borrow>;

    //!\brief Distinct from empty_t, so that uend and subrange_last can both share the address of subrange_begin.
    struct no_last_t
    {
        constexpr no_last_t() noexcept = default;
        constexpr no_last_t(auto &&) noexcept {}
    };

    [[no_unique_address]] std::conditional_t<store_end, USen, empty_t>   uend{};
    [[no_unique_address]] UIt                                            subrange_begin{};
    [[no_unique_address]] std::conditional_t<store_last, UIt, no_last_t> subrange_last{};

    template <borrowed_mp_range Borrow2, ptrdiff_t K2>
    friend class slide_counted_iterator;

    template <typename Container>
    constexpr friend slide_counted_iterator tag_invoke(custom::rebind_iterator_tag,
                                                       slide_counted_iterator it,
                                                       Container &            container_old,
                                                       Container &            container_new)
    {
        if constexpr (store_end)
            it.uend = tag_invoke(custom::rebind_iterator_tag{}, it.uend, container_old, container_new);
        it.subrange_begin = tag_invoke(custom::rebind_iterator_tag{}, it.subrange_begin, container_old, container_new);
        if constexpr (store_last)
            it.subrange_last =
              tag_invoke(custom::rebind_iterator_tag{}, it.subrange_last, container_old, container_new);

        return it;
    }

    //!\brief The position that determines equality.
    constexpr UIt const & pos() const
    {
        if constexpr (bidi_common)
            return subrange_begin;
        else
            return subrange_last;
    }

    //!\brief Inner range for non-contiguous ranges.
    using counted_t = borrowing_rad<std::counted_iterator<UIt>,
                                    std::default_sentinel_t,
                                    std::counted_iterator<UCIt>,
                                    std::default_sentinel_t,
                                    borrowing_rad_kind::sized>;

public:
    /*!\name Associated types
     * \{
     */
    using iterator_concept = std::conditional_t<
      std::random_access_iterator<UIt>,
      std::random_access_iterator_tag,
      std::conditional_t<std::bidirectional_iterator<UIt>, std::bidirectional_iterator_tag, std::forward_iterator_tag>>;
    using iterator_category = std::input_iterator_tag;
    using value_type =
      std::conditional_t<std::contiguous_iterator<UIt>, subborrow_t<Borrow, UIt, UIt, size_t>, counted_t>;
    using difference_type = std::ranges::range_difference_t<Borrow>;
    //!\}

    /*!\name Constructors, destructor and assignments.
     * \{
     */
    constexpr slide_counted_iterator()                                           = default;
    constexpr slide_counted_iterator(slide_counted_iterator const &)             = default;
    constexpr slide_counted_iterator(slide_counted_iterator &&)                  = default;
    constexpr slide_counted_iterator & operator=(slide_counted_iterator const &) = default;
    constexpr slide_counted_iterator & operator=(slide_counted_iterator &&)      = default;

    //!\brief Construct at the beginning; O(K) if !bidi_common, O(1) otherwise.
    constexpr slide_counted_iterator(Borrow urange_) : subrange_begin{radr::begin(urange_)}
    {
        if constexpr (store_end)
            uend = radr::end(urange_);
        if constexpr (store_last)
            subrange_last = std::ranges::next(subrange_begin, K - 1, radr::end(urange_));
    }

    //!\brief Construct at the end (common ranges only); O(K) if bidi_common, O(1) otherwise.
    constexpr slide_counted_iterator(Borrow urange_, std::default_sentinel_t)
        requires(!store_end)
    {
        if constexpr (bidi_common)
        {
            subrange_begin = std::ranges::prev(radr::end(urange_), K - 1, radr::begin(urange_));
        }
        else // subrange_begin is never used
        {
            subrange_begin = radr::end(urange_);
            subrange_last  = radr::end(urange_);
        }
    }

    //!\brief Construct from compatible iterator, in particular non-const to const.
    template <different_from<Borrow> Borrow2>
        requires(std::constructible_from<UIt, typename slide_counted_iterator<Borrow2, K>::UIt> &&
                 std::constructible_from<USen, typename slide_counted_iterator<Borrow2, K>::USen>)
    constexpr slide_counted_iterator(slide_counted_iterator<Borrow2, K> mut_iter) :
      uend{std::move(mut_iter.uend)},
      subrange_begin{std::move(mut_iter.subrange_begin)},
      subrange_last{std::move(mut_iter.subrange_last)}
    {}
    //!\}

    /*!\name Iterator operators
     * \{
     */
    constexpr value_type operator*() const
    {
        if constexpr (std::contiguous_iterator<UIt>)
            return subborrow(Borrow{}, subrange_begin, subrange_begin + K, static_cast<size_t>(K));
        else
            return counted_t{
              std::counted_iterator{subrange_begin, K},
              std::default_sentinel
            };
    }

    constexpr slide_counted_iterator & operator++()
    {
        ++subrange_begin;
        if constexpr (store_last)
            ++subrange_last;
        return *this;
    }

    constexpr slide_counted_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr slide_counted_iterator & operator--()
        requires std::bidirectional_iterator<UIt>
    {
        --subrange_begin;
        if constexpr (store_last)
            --subrange_last;
        return *this;
    }

    constexpr slide_counted_iterator operator--(int)
        requires std::bidirectional_iterator<UIt>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr slide_counted_iterator & operator+=(difference_type k)
        requires std::random_access_iterator<UIt>
    {
        subrange_begin += k;
        if constexpr (store_last)
            subrange_last += k;
        return *this;
    }

    constexpr slide_counted_iterator & operator-=(difference_type k)
        requires std::random_access_iterator<UIt>
    {
        subrange_begin -= k;
        if constexpr (store_last)
            subrange_last -= k;
        return *this;
    }

    constexpr value_type operator[](difference_type k) const
        requires std::random_access_iterator<UIt>
    {
        return *(*this + k);
    }

    friend constexpr slide_counted_iterator operator+(slide_counted_iterator it, difference_type k)
        requires std::random_access_iterator<UIt>
    {
        it += k;
        return it;
    }

    friend constexpr slide_counted_iterator operator+(difference_type k, slide_counted_iterator it)
        requires std::random_access_iterator<UIt>
    {
        it += k;
        return it;
    }

    friend constexpr slide_counted_iterator operator-(slide_counted_iterator it, difference_type k)
        requires std::random_access_iterator<UIt>
    {
        it -= k;
        return it;
    }

    friend constexpr difference_type operator-(slide_counted_iterator const & x, slide_counted_iterator const & y)
        requires std::sized_sentinel_for<UIt, UIt>
    {
        return x.pos() - y.pos();
    }

    friend constexpr difference_type operator-(std::default_sentinel_t, slide_counted_iterator const & x)
        requires(store_end && std::sized_sentinel_for<USen, UIt>)
    {
        return x.uend - x.subrange_last;
    }

    friend constexpr difference_type operator-(slide_counted_iterator const & x, std::default_sentinel_t)
        requires(store_end && std::sized_sentinel_for<USen, UIt>)
    {
        return x.subrange_last - x.uend;
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    friend constexpr bool operator==(slide_counted_iterator const & x, slide_counted_iterator const & y)
    {
        return x.pos() == y.pos();
    }

    friend constexpr bool operator==(slide_counted_iterator const & x, std::default_sentinel_t)
        requires store_end
    {
        return x.subrange_last == x.uend;
    }

    friend constexpr auto operator<=>(slide_counted_iterator const & x, slide_counted_iterator const & y)
        requires std::random_access_iterator<UIt>
    {
        if constexpr (std::three_way_comparable<UIt>)
            return x.pos() <=> y.pos();
        else // e.g. std::deque::iterator in libc++17
            return (x.pos() - y.pos()) <=> 0;
    }
    //!\}
};

} // namespace radr::detail

#if RADR_BUG_GCC_CONCEPT_RECURSION
template <typename Borrow, ptrdiff_t K>
struct std::iterator_traits<radr::detail::slide_counted_iterator<Borrow, K>>
{
    using iterator_concept  = typename radr::detail::slide_counted_iterator<Borrow, K>::iterator_concept;
    using iterator_category = std::input_iterator_tag;
    using value_type        = typename radr::detail::slide_counted_iterator<Borrow, K>::value_type;
    using difference_type   = typename radr::detail::slide_counted_iterator<Borrow, K>::difference_type;
    using pointer           = void;
    using reference         = value_type;
};
#endif

namespace radr::detail
{

template <ptrdiff_t K>
inline constexpr auto slide_counted_borrow = []<borrowed_mp_range URange>(URange && urange)
{
    static_assert(K > 0, "You must select K > 0 for radr::slide_counted.");

    auto borrow_ = radr::borrow(urange);

    // the resulting outer range's size (the number of windows).
    auto new_size = [&]()
    {
        if constexpr (std::ranges::sized_range<URange>)
        {
            auto       usize = std::ranges::size(urange);
            auto const k     = static_cast<decltype(usize)>(K);
            return usize >= k ? usize - k + 1 : 0;
        }
        else
        {
            return not_size{};
        }
    }();

    static constexpr auto kind =
      std::ranges::sized_range<URange> ? borrowing_rad_kind::sized : borrowing_rad_kind::unsized;

    using It  = slide_counted_iterator<decltype(borrow_), K>;
    using CIt = slide_counted_iterator<borrow_t<std::remove_cvref_t<URange> const &>, K>;

    if constexpr (common_range<URange>)
    {
        return borrowing_rad<It, It, CIt, CIt, kind>{
          It{borrow_},
          It{borrow_, std::default_sentinel},
          new_size
        };
    }
    else // iterator stores the end
    {
        using Sen = std::conditional_t<infinite_mp_range<URange>, std::unreachable_sentinel_t, std::default_sentinel_t>;
        return borrowing_rad<It, Sen, CIt, Sen, kind>{It{borrow_}, Sen{}, new_size};
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{

/*!\brief radr::slide_counted<K>(urange)
 * \tparam K The size of the windows (must be > 0).
 * \param urange The underlying range.
 * \details
 *
 * Turns a range into a range-of-ranges, by splitting it into consecutive, overlapping
 * subranges of size \p K, each shifted by 1.
 *
 * The produced elements are identical to those of radr::slide, but the types are weaker. On the other hand,
 * this adaptor can be faster than radr::slide (and std::views::slide), because the compiler can reliably unroll loops
 * over a window.
 *
 * |                           |    radr::slide                    | radr::slide_counted        |
 * |---------------------------|-----------------------------------|----------------------------|
 * | window size               | runtime parameter                 | compile-time parameter     |
 * | inner range               | always created by radr::subborrow | uses std::counted_iterator |
 * | iterator size             | larger                            | smaller                    |
 *
 * In general, prefer radr::slide for the better types. Use radr::slide_counted only if the window size is known at
 * compile-time and small, *and* if you are iterating over the window in a tight loop.
 *
 * ## Multi-pass ranges
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *
 * The returned "outer range"-type preserves from the underlying range:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::sized_range
 *   * radr::common_range
 *   * radr::mutable_range
 *   * radr::constant_range
 *   * radr::infinite_mp_range
 *
 * In general, the returned "inner range" is a pair of std::counted_iterator and std::default_sentinel_t. It always
 * models std::ranges::borrowed_range and std::ranges::sized_range, but radr::common_range only if the underlying
 * range is random-access (via radr::end).
 *
 * It preserves from the underlying range:
 *   * categories up to std::ranges::contiguous_range
 *   * radr::mutable_range
 *   * radr::constant_range
 *
 * As a special case, if the underlying range is contiguous, the returned "inner range" is created via the
 * radr::subborrow customisation point, and the behaviour is identical to radr::slide. This optimisation is not applied
 * for ranges that are random access but not contiguous, because the compiler cannot derive the trip count of a loop
 * over e.g. std::deque iterators from (it, it + K).
 *
 * Construction of the adaptor is in O(K), because the end of the first window (or, for bidirectional+common
 * underlying ranges, the begin of the end position) is searched on construction.
 *
 * ### Performance
 *
 * Considerably faster than std::views::slide and radr::slide on small windows sizes and especially on underlying
 * ranges that are random access but not contiguous.
 *
 * Iterator size is smaller or equal to std::views:slide.
 *
 * ## Single-pass ranges
 *
 * radr::slide_counted cannot be created on single-pass ranges.
 */
template <ptrdiff_t K>
inline constexpr auto slide_counted = detail::pipe_without_args_fn<void, decltype(detail::slide_counted_borrow<K>)>{};

} // namespace cpo
} // namespace radr
