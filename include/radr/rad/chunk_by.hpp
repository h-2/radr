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

#include <functional>
#include <iterator>
#include <ranges>
#include <utility>

#include "../detail/semiregular_box.hpp"
#include "../rad/chunk.hpp"

namespace radr::detail
{

template <typename Func, typename Iter>
concept chunk_by_func_constraints = object<Func> && std::indirect_binary_predicate<Func const, Iter, Iter>;

/*!\brief BoundaryFinder for radr::chunk_by: the first adjacent pair for which the predicate returns false.
 */
template <typename Pred>
struct chunk_by_finder
{
    [[no_unique_address]] semiregular_box<Pred> pred{};

    template <std::forward_iterator UIt, std::sentinel_for<UIt> USen>
        requires chunk_by_func_constraints<Pred, UIt>
    constexpr UIt find_end(UIt first, USen last) const
    {
        if (first == last)
            return first;

        UIt next = first;
        for (++next; next != last && std::invoke(*pred, *first, *next); ++next)
            first = next;

        return next;
    }

    //!\brief Find the start of the chunk that ends at \p end, searching backwards no further than \p begin.
    template <std::bidirectional_iterator UIt>
        requires chunk_by_func_constraints<Pred, UIt>
    constexpr UIt find_start(UIt end, UIt begin) const
    {
        if (end == begin)
            return end;

        UIt current = std::ranges::prev(end);
        while (current != begin)
        {
            UIt before = std::ranges::prev(current);
            if (!std::invoke(*pred, *before, *current))
                break;
            current = before;
        }
        return current;
    }
};

inline constexpr auto chunk_by_borrow = []<borrowed_mp_range URange, typename Pred>(URange && urange, Pred pred)
{
    static_assert(chunk_by_func_constraints<Pred, iterator_t<URange>> &&
                    chunk_by_func_constraints<Pred, const_iterator_t<URange>>,
                  "The constraints for radr::chunk_by's predicate are not satisfied.");

    auto borrow_ = radr::borrow(urange);
    auto finder_ = chunk_by_finder<Pred>{
      semiregular_box<Pred>{std::in_place, std::move(pred)}
    };

    if constexpr (std::ranges::bidirectional_range<URange> && common_range<URange>)
    {
        auto last_chunk_start = finder_.find_start(radr::end(borrow_), radr::begin(borrow_));

        auto it  = bidi_chunk_like_iterator{borrow_, finder_};
        auto sen = bidi_chunk_like_iterator{borrow_, finder_, std::default_sentinel, last_chunk_start};

        using It  = decltype(it);
        using CIt = bidi_chunk_like_iterator<borrow_t<std::remove_cvref_t<URange> const &>, decltype(finder_)>;

        return borrowing_rad<It, It, CIt, CIt>{std::move(it), std::move(sen)};
    }
    else
    {
        // range is un-common → it stores the end
        auto it = unidi_chunk_like_iterator{borrow_, finder_};

        using It  = decltype(it);
        using Sen = std::conditional_t<infinite_mp_range<URange>, std::unreachable_sentinel_t, std::default_sentinel_t>;
        using CIt = unidi_chunk_like_iterator<borrow_t<std::remove_cvref_t<URange> const &>, decltype(finder_)>;
        using CSen = Sen;

        return borrowing_rad<It, Sen, CIt, CSen>{std::move(it), Sen{}};
    }
};

inline constexpr auto chunk_by_coro = []<std::ranges::input_range URange, typename Pred>(URange && urange, Pred pred)
{
    static_assert(!container_lvalue<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);
    static_assert(
      std::indirect_binary_predicate<Pred &, std::ranges::iterator_t<URange>, std::ranges::iterator_t<URange>>,
      "The constraints for radr::chunk_by's predicate are not satisfied.");

    using inner_gen_t = radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>;
    using value_t     = std::ranges::range_value_t<URange>;

    return [](auto urange_, Pred pred_) -> radr::generator<inner_gen_t &>
    {
        auto it = radr::begin(urange_);
        auto e  = radr::end(urange_);

        auto inner_functor = [&pred_](auto & it_, auto & e_) -> inner_gen_t
        {
            value_t prev = *it_;
            co_yield *it_;
            ++it_;

            while (it_ != e_ && std::invoke(pred_, prev, *it_))
            {
                prev = *it_;
                co_yield *it_;
                ++it_;
            }
        };

        while (it != e)
        {
            auto tmp = inner_functor(it, e);
            co_yield tmp;
        }
    }(std::move(urange), std::move(pred));
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{

/*!\brief radr::chunk_by(urange, pred)
 * \tparam Opt Whether to optimise for size (default, via radr::chunk_by) or for range capabilities.
 * \tparam URange Type of \p urange.
 * \tparam Pred Type of \p pred.
 * \param urange The underlying range.
 * \param pred The binary predicate used to decide chunk boundaries.
 * \details
 *
 * Turns a range into a range-of-ranges, by splitting it into consecutive, non-overlapping
 * subranges between each pair of adjacent elements for which \p pred returns `false`. The
 * first element of such a pair belongs to the previous chunk, the second to the next one.
 *
 * ## Multi-pass ranges
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * `std::indirect_binary_predicate<Pred const &, radr::iterator_t<URange>, radr::iterator_t<URange>>`
 *   * `std::indirect_binary_predicate<Pred const &, radr::const_iterator_t<URange>, radr::const_iterator_t<URange>>`
 *
 * The returned "outer range"-type preserves from the underlying range:
 *   * categories up to std::ranges::bidirectional_range (only if also common)
 *   * radr::common_range (only if also bidirectional)
 *   * radr::mutable_range
 *   * radr::constant_range
 *   * radr::infinite_mp_range
 *
 * The outer range is never sized or random-access.
 *
 * The returned "inner range"-type is created via the radr::subborrow customisation point.
 * Unless customised otherwise, it always models:
 *   * std::ranges::borrowed_range
 *   * radr::common_range
 *
 * It preserves from the underlying range:
 *   * categories up to std::ranges::contiguous_range
 *   * radr::mutable_range
 *   * radr::constant_range
 *
 * It models std::ranges::sized_range iff the underlying range's iterator models std::sized_sentinel_for
 * itself (in practice: random-access ranges). This is a difference to radr::chunk, whose inner range is always sized.
 *
 * Construction of the adaptor is in O(n), because the first inner range is searched on construction.
 *
 * ### Notable differences to std::views::chunk_by
 *
 *  * std::ranges::chunk_by never preserves std::ranges::borrowed_range, this adaptor does.
 *
 * ## Single-pass ranges
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *   * `std::indirect_binary_predicate<Pred &, std::ranges::iterator_t<URange>, std::ranges::iterator_t<URange>>`
 *
 * Both, the "outer range"-type and the "inner range"-type are a radr::generator. This design is fully lazy; no
 * read-ahead happens beyond the single element required to evaluate \p pred for the next boundary.
 *
 * In contrast to the multi-pass adaptor, the semantic requirement of `std::indirect_binary_predicate` with regards
 * to regularity is waived, i.e. you may pass a function object that changes after being evaluated.
 */
inline constexpr auto chunk_by = detail::pipe_with_args_fn{detail::chunk_by_coro, detail::chunk_by_borrow};

} // namespace cpo
} // namespace radr
