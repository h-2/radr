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
#include <iterator>
#include <ranges>
#include <utility>

#include "radr/custom/subborrow.hpp"
#include "radr/detail/detail.hpp"
#include "radr/detail/fwd.hpp"
#include "radr/detail/pipe.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{

/*!\brief The forward/bidirectional iterator of radr::slide.
 * \tparam Borrow The (borrowed) underlying range.
 * \tparam Diff Type of the window size.
 * \details
 *
 * Stores the first and the last (inclusive) element of the window, so that neither ever moves past the
 * underlying end. Equality is determined by subrange_last; the end position has subrange_last == uend.
 * uend is only stored if the underlying range is not common.
 */
template <borrowed_mp_range Borrow, std::integral Diff>
class slide_iterator
{
private:
    using UIt  = iterator_t<Borrow>;
    using USen = sentinel_t<Borrow>;

    static constexpr bool store_end = !common_range<Borrow>;

    [[no_unique_address]] UIt                                          subrange_begin{};
    [[no_unique_address]] UIt                                          subrange_last{};
    [[no_unique_address]] std::conditional_t<store_end, USen, empty_t> uend{};
    [[no_unique_address]] Diff                                         k{};

    template <borrowed_mp_range Borrow2, std::integral Diff2>
    friend class slide_iterator;

    template <typename Container>
    constexpr friend slide_iterator tag_invoke(custom::rebind_iterator_tag,
                                               slide_iterator it,
                                               Container &    container_old,
                                               Container &    container_new)
    {
        it.subrange_begin = tag_invoke(custom::rebind_iterator_tag{}, it.subrange_begin, container_old, container_new);
        it.subrange_last  = tag_invoke(custom::rebind_iterator_tag{}, it.subrange_last, container_old, container_new);
        if constexpr (store_end)
            it.uend = tag_invoke(custom::rebind_iterator_tag{}, it.uend, container_old, container_new);

        return it;
    }

public:
    /*!\name Associated types
     * \{
     */
    // clang-format off
    using iterator_concept = std::conditional_t<std::random_access_iterator<UIt>, std::random_access_iterator_tag,
                             std::conditional_t<std::bidirectional_iterator<UIt>, std::bidirectional_iterator_tag,
                                                                                  std::forward_iterator_tag>>;
    // clang-format on
    using iterator_category = std::input_iterator_tag;
    using value_type        = subborrow_t<Borrow, UIt, UIt, size_t>;
    using difference_type   = std::ranges::range_difference_t<Borrow>;
    //!\}

    /*!\name Constructors, destructor and assignments.
     * \{
     */
    constexpr slide_iterator()                                   = default;
    constexpr slide_iterator(slide_iterator const &)             = default;
    constexpr slide_iterator(slide_iterator &&)                  = default;
    constexpr slide_iterator & operator=(slide_iterator const &) = default;
    constexpr slide_iterator & operator=(slide_iterator &&)      = default;

    //!\brief Construct at the beginning; O(k).
    constexpr slide_iterator(Borrow urange_, Diff k_) :
      subrange_begin{radr::begin(urange_)},
      subrange_last{std::ranges::next(radr::begin(urange_), k_ - 1, radr::end(urange_))},
      uend{radr::end(urange_)},
      k{k_}
    {}

    //!\brief Construct at the end (common ranges only); O(k) if bidirectional, O(1) otherwise.
    constexpr slide_iterator(Borrow urange_, Diff k_, std::default_sentinel_t)
        requires(!store_end)
      : subrange_begin{radr::end(urange_)}, subrange_last{radr::end(urange_)}, k{k_}
    {
        // subrange_begin is only needed for operator--
        if constexpr (std::bidirectional_iterator<UIt>)
            std::ranges::advance(subrange_begin, -static_cast<difference_type>(k - 1), radr::begin(urange_));
    }

    //!\brief Construct from compatible iterator, in particular non-const to const.
    template <different_from<Borrow> Borrow2>
        requires std::constructible_from<UIt, typename slide_iterator<Borrow2, Diff>::UIt>
    constexpr slide_iterator(slide_iterator<Borrow2, Diff> mut_iter) :
      subrange_begin{std::move(mut_iter.subrange_begin)},
      subrange_last{std::move(mut_iter.subrange_last)},
      uend{std::move(mut_iter.uend)},
      k{mut_iter.k}
    {}
    //!\}

    /*!\name Iterator operators
     * \{
     */
    constexpr value_type operator*() const
    {
        return subborrow(Borrow{}, subrange_begin, std::ranges::next(subrange_last), static_cast<size_t>(k));
    }

    constexpr slide_iterator & operator++()
    {
        ++subrange_begin;
        ++subrange_last;
        return *this;
    }

    constexpr slide_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr slide_iterator & operator--()
        requires std::bidirectional_iterator<UIt>
    {
        --subrange_begin;
        --subrange_last;
        return *this;
    }

    constexpr slide_iterator operator--(int)
        requires std::bidirectional_iterator<UIt>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr slide_iterator & operator+=(difference_type i)
        requires std::random_access_iterator<UIt>
    {
        subrange_begin += i;
        subrange_last += i;
        return *this;
    }

    constexpr slide_iterator & operator-=(difference_type i)
        requires std::random_access_iterator<UIt>
    {
        subrange_begin -= i;
        subrange_last -= i;
        return *this;
    }

    constexpr value_type operator[](difference_type i) const
        requires std::random_access_iterator<UIt>
    {
        return *(*this + i);
    }

    friend constexpr slide_iterator operator+(slide_iterator it, difference_type i)
        requires std::random_access_iterator<UIt>
    {
        it += i;
        return it;
    }

    friend constexpr slide_iterator operator+(difference_type i, slide_iterator it)
        requires std::random_access_iterator<UIt>
    {
        it += i;
        return it;
    }

    friend constexpr slide_iterator operator-(slide_iterator it, difference_type i)
        requires std::random_access_iterator<UIt>
    {
        it -= i;
        return it;
    }

    friend constexpr difference_type operator-(slide_iterator const & x, slide_iterator const & y)
        requires std::sized_sentinel_for<UIt, UIt>
    {
        return x.subrange_last - y.subrange_last;
    }

    friend constexpr difference_type operator-(std::default_sentinel_t, slide_iterator const & x)
        requires(store_end && std::sized_sentinel_for<USen, UIt>)
    {
        return x.uend - x.subrange_last;
    }

    friend constexpr difference_type operator-(slide_iterator const & x, std::default_sentinel_t)
        requires(store_end && std::sized_sentinel_for<USen, UIt>)
    {
        return x.subrange_last - x.uend;
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    friend constexpr bool operator==(slide_iterator const & x, slide_iterator const & y)
    {
        return x.subrange_last == y.subrange_last;
    }

    friend constexpr bool operator==(slide_iterator const & x, std::default_sentinel_t)
        requires store_end
    {
        return x.subrange_last == x.uend;
    }

    friend constexpr auto operator<=>(slide_iterator const & x, slide_iterator const & y)
        requires std::random_access_iterator<UIt>
    {
        if constexpr (std::three_way_comparable<UIt>)
            return x.subrange_last <=> y.subrange_last;
        else // e.g. std::deque::iterator in libc++17
            return (x.subrange_last - y.subrange_last) <=> 0;
    }
    //!\}
};

} // namespace radr::detail

#if RADR_BUG_GCC_CONCEPT_RECURSION
template <typename Borrow, typename Diff>
struct std::iterator_traits<radr::detail::slide_iterator<Borrow, Diff>>
{
    using iterator_concept  = typename radr::detail::slide_iterator<Borrow, Diff>::iterator_concept;
    using iterator_category = std::input_iterator_tag;
    using value_type        = typename radr::detail::slide_iterator<Borrow, Diff>::value_type;
    using difference_type   = typename radr::detail::slide_iterator<Borrow, Diff>::difference_type;
    using pointer           = void;
    using reference         = value_type;
};
#endif

namespace radr::detail
{

inline constexpr auto slide_borrow = []<borrowed_mp_range URange, std::integral Diff>(URange && urange, Diff k)
{
    assert(k > 0 && "radr::slide requires k > 0.");

    auto borrow_ = radr::borrow(urange);

    // the resulting outer range's size (the number of windows).
    auto new_size = [&]()
    {
        if constexpr (std::ranges::sized_range<URange>)
        {
            auto usize = std::ranges::size(urange);
            auto i     = static_cast<decltype(usize)>(k);
            return usize >= i ? usize - i + 1 : 0;
        }
        else
        {
            return not_size{};
        }
    }();

    static constexpr auto kind =
      std::ranges::sized_range<URange> ? borrowing_rad_kind::sized : borrowing_rad_kind::unsized;

    // TODO dedicated iterator for random_access_range && sized_range
    using It  = slide_iterator<decltype(borrow_), Diff>;
    using CIt = slide_iterator<borrow_t<std::remove_cvref_t<URange> const &>, Diff>;

    if constexpr (common_range<URange>)
    {
        return borrowing_rad<It, It, CIt, CIt, kind>{
          It{borrow_, k},
          It{borrow_, k, std::default_sentinel},
          new_size
        };
    }
    else // iterator stores the end
    {
        using Sen = std::conditional_t<infinite_mp_range<URange>, std::unreachable_sentinel_t, std::default_sentinel_t>;
        return borrowing_rad<It, Sen, CIt, Sen, kind>{
          It{borrow_, k},
          Sen{},
          new_size
        };
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{

/*!\brief radr::slide(urange, k)
 * \tparam URange Type of \p urange.
 * \param urange The underlying range.
 * \param k The size of the chunks/windows (must be > 0).
 * \details
 *
 * Turns a range into a range-of-ranges, by splitting it into consecutive, overlapping
 * subranges of size \p k, each shifted by 1. For texts, this generates what is commonly
 * referred to as the "n-grams" or "k-mers".
 *
 * This library provides a second adaptor with similar semantics: radr::slide_counted.
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
 *   * radr::inifite_mp_range
 *
 * The returned "inner range" (the window) is created via the radr::subborrow customisation point.
 * It has the type `radr::subborrow_t<URange, UIt, UIt, size_t>`. Unless customised otherwise, it always models:
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::common_range
 *
 * And it preserves from the underlying range:
 *   * categories up to std::ranges::contiguous_range
 *   * radr::mutable_range
 *   * radr::constant_range
 *
 * Construction of the adaptor is in O(k), because the first inner range is searched on construction.
 *
 * ### Notable differences to std::views::slide
 *
 *  * std::ranges::slide never preserves std::ranges::borrowed_range, this adaptor does.
 *  * std::ranges::slide does not preserve radr::common_range for forward ranges, this adaptor does.
 *
 * ## Single-pass ranges
 *
 * radr::slide cannot be created on single-pass ranges.
 *
 */
inline constexpr auto slide = detail::pipe_with_args_fn<void, decltype(detail::slide_borrow)>{};

} // namespace cpo
} // namespace radr
