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

#include "radr/custom/subborrow.hpp"
#include "radr/detail/detail.hpp"
#include "radr/detail/fwd.hpp"
#include "radr/detail/pipe.hpp"
#include "radr/rad/chunk.hpp"
#include "radr/rad/take.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{

/*!\brief Forward iterator that derives the current chunk lazily on dereference.
 * \tparam Borrow The (borrowed) underlying range.
 */
template <borrowed_mp_range Borrow>
class lazy_chunk_iterator
{
private:
    using UIt   = iterator_t<Borrow>;
    using USen  = sentinel_t<Borrow>;
    using UCIt  = const_iterator_t<Borrow>;
    using UCSen = const_sentinel_t<Borrow>;
    using Diff  = std::ranges::range_difference_t<Borrow>;

    [[no_unique_address]] USen uend{};
    [[no_unique_address]] UIt  subrange_begin{};
    [[no_unique_address]] Diff n{};

    template <borrowed_mp_range Borrow2>
    friend class lazy_chunk_iterator;

    template <typename Container>
    constexpr friend lazy_chunk_iterator tag_invoke(custom::rebind_iterator_tag,
                                                    lazy_chunk_iterator it,
                                                    Container &         container_old,
                                                    Container &         container_new)
    {
        it.uend           = tag_invoke(custom::rebind_iterator_tag{}, it.uend, container_old, container_new);
        it.subrange_begin = tag_invoke(custom::rebind_iterator_tag{}, it.subrange_begin, container_old, container_new);

        return it;
    }

public:
    /*!\name Associated types
     * \{
     */
    using iterator_concept  = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type        = borrowing_rad<std::counted_iterator<UIt>,
                                     take_sentinel<UIt, USen>,
                                     std::counted_iterator<UCIt>,
                                     take_sentinel<UCIt, UCSen>,
                                     borrowing_rad_kind::unsized>;
    using difference_type   = Diff;
    //!\}

    /*!\name Constructors, destructor and assignments.
     * \{
     */
    constexpr lazy_chunk_iterator()                                        = default;
    constexpr lazy_chunk_iterator(lazy_chunk_iterator const &)             = default;
    constexpr lazy_chunk_iterator(lazy_chunk_iterator &&)                  = default;
    constexpr lazy_chunk_iterator & operator=(lazy_chunk_iterator const &) = default;
    constexpr lazy_chunk_iterator & operator=(lazy_chunk_iterator &&)      = default;

    //!\brief Construct at the beginning.
    constexpr lazy_chunk_iterator(Borrow urange_, Diff n_) :
      uend{radr::end(urange_)}, subrange_begin{radr::begin(urange_)}, n{n_}
    {}

    //!\brief Construct from compatible iterator, in particular non-const to const.
    template <different_from<Borrow> Borrow2>
        requires(std::constructible_from<UIt, typename lazy_chunk_iterator<Borrow2>::UIt> &&
                 std::constructible_from<USen, typename lazy_chunk_iterator<Borrow2>::USen>)
    constexpr lazy_chunk_iterator(lazy_chunk_iterator<Borrow2> mut_iter) :
      uend{std::move(mut_iter.uend)}, subrange_begin{std::move(mut_iter.subrange_begin)}, n{mut_iter.n}
    {}
    //!\}

    /*!\name Iterator operators
     * \{
     */
    constexpr value_type operator*() const
    {
        return {
          std::counted_iterator{subrange_begin, n},
          take_sentinel<UIt, USen>{uend}
        };
    }

    constexpr lazy_chunk_iterator & operator++()
    {
        subrange_begin = std::ranges::next(subrange_begin, n, uend);
        return *this;
    }

    constexpr lazy_chunk_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    friend constexpr bool operator==(lazy_chunk_iterator const & x, lazy_chunk_iterator const & y)
    {
        return x.subrange_begin == y.subrange_begin;
    }

    //!\brief The iterator already carries `uend` itself.
    friend constexpr bool operator==(lazy_chunk_iterator const & x, std::default_sentinel_t)
    {
        return x.subrange_begin == x.uend;
    }
    //!\}
};

} // namespace radr::detail

#if RADR_BUG_GCC_CONCEPT_RECURSION
template <typename Borrow>
struct std::iterator_traits<radr::detail::lazy_chunk_iterator<Borrow>>
{
    using iterator_concept  = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type        = typename radr::detail::lazy_chunk_iterator<Borrow>::value_type;
    using difference_type   = typename radr::detail::lazy_chunk_iterator<Borrow>::difference_type;
    using pointer           = void;
    using reference         = value_type;
};
#endif

namespace radr::detail
{

inline constexpr auto lazy_chunk_borrow = []<borrowed_mp_range URange, std::integral Diff>(URange && urange, Diff n)
{
    assert(n > 0 && "radr::lazy_chunk requires n > 0.");

    auto borrow_ = radr::borrow(urange);

    using RDiff = std::ranges::range_difference_t<URange>;

    auto it = lazy_chunk_iterator{borrow_, static_cast<RDiff>(n)};

    using It   = decltype(it);
    using Sen  = std::conditional_t<infinite_mp_range<URange>, std::unreachable_sentinel_t, std::default_sentinel_t>;
    using CIt  = lazy_chunk_iterator<borrow_t<std::remove_cvref_t<URange> const &>>;
    using CSen = Sen;

    if constexpr (std::ranges::sized_range<URange>)
    {
        auto new_size = (std::ranges::size(urange) + n - 1) / n; // integer division, but rounding up
        return borrowing_rad<It, Sen, CIt, CSen, borrowing_rad_kind::sized>{std::move(it), Sen{}, new_size};
    }
    else
    {
        return borrowing_rad<It, Sen, CIt, CSen>{std::move(it), Sen{}};
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{

/*!\brief radr::lazy_chunk(urange, n)
 * \tparam URange Type of \p urange.
 * \param urange The underlying range.
 * \param n The size of the chunks (must be > 0).
 * \details
 *
 * Turns a range into a range-of-ranges, by chunking it into consecutive, non-overlapping
 * subranges of size \p n each, except possibly the last one, which is shorter if the size
 * of \p urange is not a multiple of \p n.
 *
 * The results are identical to those of radr::chunk; only the types differ. See below on how to choose.
 *
 * ## Multi-pass ranges
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *
 * The returned "outer range"-type always models std::ranges::forward_range and never anything stronger; it is
 * never a radr::common_range. It preserves from the underlying range:
 *   * std::ranges::sized_range
 *   * radr::mutable_range
 *   * radr::constant_range
 *   * radr::infinite_mp_range
 *
 * The returned "inner range" is NOT created using the radr::subborrow customistion point, and its type never models
 * std::ranges::sized_range or radr::common_range. It always models std::ranges::borrowed_range, and it preserves
 * from the underlying range:
 *   * categories up to std::ranges::contiguous_range
 *   * radr::mutable_range
 *   * radr::constant_range
 *
 * Construction of the adaptor is in O(1); no chunk boundary is searched before it is needed.
 *
 * ### Choosing between radr::chunk and radr::lazy_chunk
 *
 * radr::chunk gives you better range types:
 *   * Its outer range potentially preserves bidirectional/random-access (and with it common_range and sized_range).
 *   * Its inner range is always sized and common, so e.g. the chunks of a std::string_view are also std::string_views.
 *
 * radr::lazy_chunk produces weaker range types (for both inner and outer range), but it can be notably faster
 * if the chunk size \p n is a compile-time constant / literal, and small. The reason is that the compiler can
 * unroll the inner loop (which it cannot for radr::chunk). Additionally, the iterator-sentinel-pair of
 * radr::lazy_chunk is almost always smaller than that of radr::chunk.
 *
 * See the test-folder's integration tests and benchmarks for more details.
 *
 * Rule-of-thumb:
 *   * Use radr::chunk by default, especially in chains with other range adaptors.
 *   * Use radr::lazy_chunk only if you do a full pass over the whole range (all chunks) from begin to end,
 * **and**\p n is a small constant.
 *
 * ### Notable differences to std::views::chunk
 *
 * This adaptor behaves on all underlying ranges like std::views::chunk behaves on ranges that are not
 * random-access+sized.
 *
 * The outer range of std::views::chunk preserves bidirectional/random-access, this one never does. Use radr::chunk
 * if you want this behaviour.
 *
 * ## Single-pass ranges
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *
 * Identical to radr::chunk: both the "outer range"-type and the "inner range"-type are a radr::generator.
 * This design is fully lazy; no read-ahead happens.
 *
 */
inline constexpr auto lazy_chunk = detail::pipe_with_args_fn{detail::chunk_coro, detail::lazy_chunk_borrow};

} // namespace cpo
} // namespace radr
