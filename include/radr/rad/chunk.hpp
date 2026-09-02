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
#include <iterator>
#include <ranges>

#include "radr/custom/subborrow.hpp"
#include "radr/detail/detail.hpp"
#include "radr/detail/fwd.hpp"
#include "radr/detail/pipe.hpp"
#include "radr/generator.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{

/*!\brief Whether the BoundaryFinder knows the chunk size upfront.
 * \details
 * True for radr::chunk (radr::detail::chunk_size_finder), false for radr::chunk_by.
 */
template <typename BoundaryFinder>
concept fixed_size_finder = requires(BoundaryFinder const & f) { f.n; };

/*!\brief The size of the chunk [\p b, \p e), or radr::detail::not_size if it cannot be computed in O(1).
 * \param finder The BoundaryFinder of the calling iterator.
 * \param b Begin of the current chunk.
 * \param e End of the current chunk.
 * \param uend End of the underlying range.
 * \details
 *
 * Shared by both chunk-like iterators; the return type doubles as the size argument of radr::subborrow,
 * so it determines whether the inner range models std::ranges::sized_range (see radr::detail::chunk_size_t).
 */
template <typename BoundaryFinder, typename UIt, typename USen>
constexpr auto chunk_size([[maybe_unused]] BoundaryFinder const & finder,
                          [[maybe_unused]] UIt const &            b,
                          [[maybe_unused]] UIt const &            e,
                          [[maybe_unused]] USen const &           uend)
{
    if constexpr (std::sized_sentinel_for<UIt, UIt>)
    {
        return to_unsigned_like(e - b);
    }
    else if constexpr (fixed_size_finder<BoundaryFinder>)
    {
        // only the last chunk may be shorter than n, so the linear count happens at most once per traversal
        return to_unsigned_like(e == uend ? std::ranges::distance(b, e) : finder.n);
    }
    else
    {
        return not_size{};
    }
}

//!\brief The return type of radr::detail::chunk_size.
template <typename BoundaryFinder, typename UIt, typename USen>
using chunk_size_t = decltype(chunk_size(std::declval<BoundaryFinder const &>(),
                                         std::declval<UIt const &>(),
                                         std::declval<UIt const &>(),
                                         std::declval<USen const &>()));

/*!\brief The forward-only iterator for radr::chunk and radr::chunk_by.
 * \tparam Borrow The (borrowed) underlying range.
 * \tparam BoundaryFinder Policy object that locates the end of the next chunk.
 */
template <borrowed_mp_range Borrow, std::semiregular BoundaryFinder>
class unidi_chunk_like_iterator
{
private:
    using UIt  = iterator_t<Borrow>;
    using USen = sentinel_t<Borrow>;

    [[no_unique_address]] BoundaryFinder finder{};
    [[no_unique_address]] USen           uend{};
    [[no_unique_address]] UIt            subrange_begin{};
    [[no_unique_address]] UIt            subrange_end{};

    template <borrowed_mp_range Borrow2, std::semiregular BoundaryFinder2>
    friend class unidi_chunk_like_iterator;

    template <typename Container>
    constexpr friend unidi_chunk_like_iterator tag_invoke(custom::rebind_iterator_tag,
                                                          unidi_chunk_like_iterator it,
                                                          Container &               container_old,
                                                          Container &               container_new)
    {
        it.subrange_begin = tag_invoke(custom::rebind_iterator_tag{}, it.subrange_begin, container_old, container_new);
        it.subrange_end   = tag_invoke(custom::rebind_iterator_tag{}, it.subrange_end, container_old, container_new);
        it.uend           = tag_invoke(custom::rebind_iterator_tag{}, it.uend, container_old, container_new);

        return it;
    }

public:
    /*!\name Associated types
     * \{
     */
    using iterator_concept  = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type        = subborrow_t<Borrow, UIt, UIt, chunk_size_t<BoundaryFinder, UIt, USen>>;
    using difference_type   = std::ranges::range_difference_t<Borrow>;
    //!\}

    /*!\name Constructors, destructor and assignments.
     * \{
     */
    constexpr unidi_chunk_like_iterator()                                              = default;
    constexpr unidi_chunk_like_iterator(unidi_chunk_like_iterator const &)             = default;
    constexpr unidi_chunk_like_iterator(unidi_chunk_like_iterator &&)                  = default;
    constexpr unidi_chunk_like_iterator & operator=(unidi_chunk_like_iterator const &) = default;
    constexpr unidi_chunk_like_iterator & operator=(unidi_chunk_like_iterator &&)      = default;

    //!\brief Construct from values.
    constexpr unidi_chunk_like_iterator(Borrow urange_, BoundaryFinder finder_) :
      finder{std::move(finder_)}, uend{radr::end(urange_)}, subrange_begin{radr::begin(urange_)}
    {
        if (subrange_begin != uend)
            subrange_end = finder.find_end(subrange_begin, uend);
        else
            subrange_end = subrange_begin;
    }

    //!\brief Construct from compatible iterator, in particular non-const to const.
    template <different_from<Borrow> Borrow2, typename BoundaryFinder2>
        requires(std::constructible_from<UIt, typename unidi_chunk_like_iterator<Borrow2, BoundaryFinder2>::UIt> &&
                 std::constructible_from<USen, typename unidi_chunk_like_iterator<Borrow2, BoundaryFinder2>::USen> &&
                 std::constructible_from<BoundaryFinder, BoundaryFinder2>)
    constexpr unidi_chunk_like_iterator(unidi_chunk_like_iterator<Borrow2, BoundaryFinder2> mut_iter) :
      finder{std::move(mut_iter.finder)},
      uend{std::move(mut_iter.uend)},
      subrange_begin{std::move(mut_iter.subrange_begin)},
      subrange_end{std::move(mut_iter.subrange_end)}
    {}
    //!\}

    /*!\name Iterator operators
     * \{
     */
    constexpr value_type operator*() const
    {
        return subborrow(Borrow{},
                         subrange_begin,
                         subrange_end,
                         chunk_size(finder, subrange_begin, subrange_end, uend));
    }

    constexpr unidi_chunk_like_iterator & operator++()
    {
        subrange_begin = subrange_end;
        if (subrange_begin != uend)
            subrange_end = finder.find_end(subrange_begin, uend);
        return *this;
    }

    constexpr unidi_chunk_like_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    friend constexpr bool operator==(unidi_chunk_like_iterator const & x, unidi_chunk_like_iterator const & y)
    {
        return x.subrange_begin == y.subrange_begin;
    }

    //!\brief The iterator already carries `uend` itself, so std::default_sentinel_t suffices as end-marker.
    friend constexpr bool operator==(unidi_chunk_like_iterator const & x, std::default_sentinel_t)
    {
        return x.subrange_begin == x.uend;
    }
    //!\}
};

} // namespace radr::detail

#if RADR_BUG_GCC_CONCEPT_RECURSION
template <typename Borrow, typename BoundaryFinder>
struct std::iterator_traits<radr::detail::unidi_chunk_like_iterator<Borrow, BoundaryFinder>>
{
    using iterator_concept  = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type        = typename radr::detail::unidi_chunk_like_iterator<Borrow, BoundaryFinder>::value_type;
    using difference_type   = typename radr::detail::unidi_chunk_like_iterator<Borrow, BoundaryFinder>::difference_type;
    using pointer           = void;
    using reference         = value_type;
};
#endif

namespace radr::detail
{

/*!\brief The bidirectional+common iterator of radr::chunk and radr::chunk_by.
 * \tparam Borrow The (borrowed) underlying range; must be std::ranges::bidirectional_range and
 *         radr::common_range.
 * \tparam BoundaryFinder Policy object that locates the end of the next / the start of the previous chunk.
 * \details
 *
 * Compared to unidi_chunk_like_iterator, this additionally stores `ubegin` (needed so that searching
 * backwards for a chunk boundary never underflows past the start of the underlying range) and models
 * radr::common_range itself.
 *
 * `BoundaryFinder` must additionally provide `UIt find_start(UIt end, UIt begin) const`.
 *
 * The last chunk is potentially smaller than n, so we cannot use the BoundaryFinder for decrementing
 * the end. Instead the calling function (which knows the size) pre-computes the begin of the last chunk
 * and stores when constructing an iterator as the sentinel.
 * The drawback of this design is an if-check in operator-- (that branch prediction hopefully covers).
 */
template <borrowed_mp_range Borrow, std::semiregular BoundaryFinder>
    requires std::ranges::bidirectional_range<Borrow> && common_range<Borrow>
class bidi_chunk_like_iterator
{
private:
    using UIt = iterator_t<Borrow>;

    [[no_unique_address]] BoundaryFinder finder{};
    [[no_unique_address]] UIt            ubegin{};
    [[no_unique_address]] UIt            uend{};
    [[no_unique_address]] UIt            subrange_begin{};
    [[no_unique_address]] UIt            subrange_end{};

    template <borrowed_mp_range Borrow2, std::semiregular BoundaryFinder2>
        requires std::ranges::bidirectional_range<Borrow2> && common_range<Borrow2>
    friend class bidi_chunk_like_iterator;

    template <typename Container>
    constexpr friend bidi_chunk_like_iterator tag_invoke(custom::rebind_iterator_tag,
                                                         bidi_chunk_like_iterator it,
                                                         Container &              container_old,
                                                         Container &              container_new)
    {
        it.ubegin         = tag_invoke(custom::rebind_iterator_tag{}, it.ubegin, container_old, container_new);
        it.uend           = tag_invoke(custom::rebind_iterator_tag{}, it.uend, container_old, container_new);
        it.subrange_begin = tag_invoke(custom::rebind_iterator_tag{}, it.subrange_begin, container_old, container_new);
        it.subrange_end   = tag_invoke(custom::rebind_iterator_tag{}, it.subrange_end, container_old, container_new);

        return it;
    }

public:
    /*!\name Associated types
     * \{
     */
    using iterator_concept  = std::bidirectional_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type        = subborrow_t<Borrow, UIt, UIt, chunk_size_t<BoundaryFinder, UIt, UIt>>;
    using difference_type   = std::ranges::range_difference_t<value_type>;
    //!\}

    /*!\name Constructors, destructor and assignments.
     * \{
     */
    constexpr bidi_chunk_like_iterator()                                             = default;
    constexpr bidi_chunk_like_iterator(bidi_chunk_like_iterator const &)             = default;
    constexpr bidi_chunk_like_iterator(bidi_chunk_like_iterator &&)                  = default;
    constexpr bidi_chunk_like_iterator & operator=(bidi_chunk_like_iterator const &) = default;
    constexpr bidi_chunk_like_iterator & operator=(bidi_chunk_like_iterator &&)      = default;

    //!\brief Construct at the beginning.
    constexpr bidi_chunk_like_iterator(Borrow urange_, BoundaryFinder finder_) :
      finder{std::move(finder_)}, ubegin{radr::begin(urange_)}, uend{radr::end(urange_)}, subrange_begin{ubegin}
    {
        subrange_end = (subrange_begin != uend) ? finder.find_end(subrange_begin, uend) : subrange_begin;
    }

    //!\brief Construct at the end.
    //!\param last_chunk_start The start of the last chunk, precomputed by the caller.
    //!\      Temporarily stored in `subrange_end`, which is otherwise unused for the end position.
    constexpr bidi_chunk_like_iterator(Borrow         urange_,
                                       BoundaryFinder finder_,
                                       std::default_sentinel_t,
                                       UIt last_chunk_start) :
      finder{std::move(finder_)},
      ubegin{radr::begin(urange_)},
      uend{radr::end(urange_)},
      subrange_begin{uend},
      subrange_end{std::move(last_chunk_start)}
    {}

    //!\brief Construct from compatible iterator, in particular non-const to const.
    template <different_from<Borrow> Borrow2, typename BoundaryFinder2>
        requires(std::constructible_from<UIt, typename bidi_chunk_like_iterator<Borrow2, BoundaryFinder2>::UIt> &&
                 std::constructible_from<BoundaryFinder, BoundaryFinder2>)
    constexpr bidi_chunk_like_iterator(bidi_chunk_like_iterator<Borrow2, BoundaryFinder2> mut_iter) :
      finder{std::move(mut_iter.finder)},
      ubegin{std::move(mut_iter.ubegin)},
      uend{std::move(mut_iter.uend)},
      subrange_begin{std::move(mut_iter.subrange_begin)},
      subrange_end{std::move(mut_iter.subrange_end)}
    {}
    //!\}

    /*!\name Iterator operators
     * \{
     */
    constexpr value_type operator*() const
    {
        return subborrow(Borrow{},
                         subrange_begin,
                         subrange_end,
                         chunk_size(finder, subrange_begin, subrange_end, uend));
    }

    constexpr bidi_chunk_like_iterator & operator++()
    {
        subrange_begin = subrange_end;
        if (subrange_begin != uend)
            subrange_end = finder.find_end(subrange_begin, uend);
        return *this;
    }

    constexpr bidi_chunk_like_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr bidi_chunk_like_iterator & operator--()
    {
        if (subrange_begin == uend)
        {
            // The first decrement from a freshly obtained end iterator.
            // subrange_end was initialised with the start of the last chunk.
            subrange_begin = subrange_end;
            subrange_end   = uend;
        }
        else
        {
            subrange_end   = subrange_begin;
            subrange_begin = finder.find_start(subrange_end, ubegin);
        }
        return *this;
    }

    constexpr bidi_chunk_like_iterator operator--(int)
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    friend constexpr bool operator==(bidi_chunk_like_iterator const & x, bidi_chunk_like_iterator const & y)
    {
        return x.subrange_begin == y.subrange_begin;
    }
    //!\}
};

/*!\brief The random-access iterator used by radr::chunk when the underlying range is RA+sized.
 * \details
 *
 * This iterator design is an optimisation that std::views doesn't do.
 *
 * We store the underlying range's begin iterator plus three integers, rather than the
 * multiple full iterators. Because the position is an offset, operator++/-- are a single
 * addition/subtraction with no multiplication.
 *
 * In contrast to the bidi-iterator, we need no special handling of the last chunk, because
 * the generic operator* already contains all necessary logic.
 */
template <borrowed_mp_range Borrow>
    requires std::ranges::random_access_range<Borrow> && std::ranges::sized_range<Borrow>
class ra_chunk_like_iterator
{
private:
    using UIt  = iterator_t<Borrow>;
    using Diff = std::ranges::range_difference_t<Borrow>;

    [[no_unique_address]] UIt  ubegin{}; // underlying begin | immutable
    [[no_unique_address]] Diff usize{};  // underlying size  | immutable
    [[no_unique_address]] Diff n{};      // size of chunks   | immutable
    [[no_unique_address]] Diff i{};      // offset into underlying range (begin of chunk, NOT chunk index)

    template <borrowed_mp_range Borrow2>
        requires std::ranges::random_access_range<Borrow2> && std::ranges::sized_range<Borrow2>
    friend class ra_chunk_like_iterator;

    template <typename Container>
    constexpr friend ra_chunk_like_iterator tag_invoke(custom::rebind_iterator_tag,
                                                       ra_chunk_like_iterator it,
                                                       Container &            container_old,
                                                       Container &            container_new)
    {
        it.ubegin = tag_invoke(custom::rebind_iterator_tag{}, it.ubegin, container_old, container_new);
        return it;
    }

public:
    /*!\name Associated types
     * \{
     */
    using iterator_concept  = std::random_access_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type        = subborrow_t<Borrow, UIt, UIt>;
    using difference_type   = Diff;
    //!\}

    /*!\name Constructors, destructor and assignments.
     * \{
     */
    constexpr ra_chunk_like_iterator()                                           = default;
    constexpr ra_chunk_like_iterator(ra_chunk_like_iterator const &)             = default;
    constexpr ra_chunk_like_iterator(ra_chunk_like_iterator &&)                  = default;
    constexpr ra_chunk_like_iterator & operator=(ra_chunk_like_iterator const &) = default;
    constexpr ra_chunk_like_iterator & operator=(ra_chunk_like_iterator &&)      = default;

    //!\brief Construct at position.
    constexpr ra_chunk_like_iterator(Borrow urange_, Diff n_, Diff i_ = 0) :
      ubegin{radr::begin(urange_)}, usize{static_cast<Diff>(std::ranges::size(urange_))}, n{n_}, i{i_}
    {}

    //!\brief Construct from compatible iterator, in particular non-const to const.
    template <different_from<Borrow> Borrow2>
        requires std::constructible_from<UIt, typename ra_chunk_like_iterator<Borrow2>::UIt>
    constexpr ra_chunk_like_iterator(ra_chunk_like_iterator<Borrow2> mut_iter) :
      ubegin{std::move(mut_iter.ubegin)}, usize{mut_iter.usize}, n{mut_iter.n}, i{mut_iter.i}
    {}
    //!\}

    /*!\name Iterator operators
     * \{
     */
    constexpr value_type operator*() const
    {
        if constexpr (std::contiguous_iterator<UIt>)
        {
            // Two independent advances from ubegin. Equivalent to the branch below, but GCC
            // generates a denser loop body for this form; measurably faster on std::vector when
            // only few elements per chunk are read (see the "stride" benchmark).
            return subborrow(Borrow{}, ubegin + i, ubegin + std::min(i + n, usize));
        }
        else
        {
            // The end is a small step (<= n elements) from the begin iterator already computed,
            // instead of a second, independent full-magnitude advance from ubegin. For random-access
            // iterators that are not contiguous (e.g. std::deque, whose operator+= only takes its
            // cheap pointer-arithmetic path when the jump stays within the current block), the
            // second full-magnitude advance is measurably more expensive.
            auto b = ubegin + i;
            return subborrow(Borrow{}, b, b + std::min(n, usize - i));
        }
    }

    constexpr value_type operator[](difference_type k) const { return *(*this + k); }

    constexpr ra_chunk_like_iterator & operator++()
    {
        i += n;
        return *this;
    }

    constexpr ra_chunk_like_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr ra_chunk_like_iterator & operator--()
    {
        i -= n;
        return *this;
    }

    constexpr ra_chunk_like_iterator operator--(int)
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr ra_chunk_like_iterator & operator+=(difference_type k)
    {
        i += k * n;
        return *this;
    }

    constexpr ra_chunk_like_iterator & operator-=(difference_type k)
    {
        i -= k * n;
        return *this;
    }

    friend constexpr ra_chunk_like_iterator operator+(ra_chunk_like_iterator it, difference_type k)
    {
        it += k;
        return it;
    }

    friend constexpr ra_chunk_like_iterator operator+(difference_type k, ra_chunk_like_iterator it)
    {
        it += k;
        return it;
    }

    friend constexpr ra_chunk_like_iterator operator-(ra_chunk_like_iterator it, difference_type k)
    {
        it -= k;
        return it;
    }

    friend constexpr difference_type operator-(ra_chunk_like_iterator const & x, ra_chunk_like_iterator const & y)
    {
        return (x.i - y.i) / x.n;
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    friend constexpr bool operator==(ra_chunk_like_iterator const & x, ra_chunk_like_iterator const & y)
    {
        return x.i == y.i;
    }

    friend constexpr auto operator<=>(ra_chunk_like_iterator const & x, ra_chunk_like_iterator const & y)
    {
        return x.i <=> y.i;
    }
    //!\}
};

/*!\brief BoundaryFinder for radr::chunk: every n-th element is a boundary.
 */
template <std::integral Diff>
struct chunk_size_finder
{
    Diff n{};

    template <typename UIt, typename USen>
    constexpr UIt find_end(UIt begin, USen end) const
    {
        return std::ranges::next(std::move(begin), n, end);
    }

    template <std::bidirectional_iterator UIt>
    constexpr UIt find_start(UIt end, UIt begin) const
    {
        return std::ranges::prev(std::move(end), n, begin);
    }
};

inline constexpr auto chunk_borrow = []<borrowed_mp_range URange, std::integral Diff>(URange && urange, Diff n)
{
    assert(n > 0 && "radr::chunk requires n > 0.");

    auto borrow_ = radr::borrow(urange);

    // the underlying range's own size.
    auto usize = [&]()
    {
        if constexpr (std::ranges::sized_range<URange>)
            return std::ranges::size(urange);
        else
            return not_size{};
    }();

    // the resulting outer range's size (the number of chunks).
    auto new_size = [&]()
    {
        if constexpr (std::ranges::sized_range<URange>)
            return (usize + n - 1) / n; // integer division, but rounding up
        else
            return not_size{};
    }();

    if constexpr (std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
    {
        using RDiff = std::ranges::range_difference_t<URange>;
        auto n_     = static_cast<RDiff>(n);

        auto it  = ra_chunk_like_iterator{borrow_, n_, static_cast<RDiff>(0)};
        auto sen = ra_chunk_like_iterator{borrow_, n_, static_cast<RDiff>(new_size) * n_};

        using It  = decltype(it);
        using CIt = ra_chunk_like_iterator<borrow_t<std::remove_cvref_t<URange> const &>>;

        return borrowing_rad<It, It, CIt, CIt>{std::move(it), std::move(sen)};
    }
    else if constexpr (std::ranges::bidirectional_range<URange> && common_range<URange> &&
                       std::ranges::sized_range<URange>)
    {
        auto finder_ = chunk_size_finder{n};

        // length of the last chunk which is potentially shorter than n
        // 0 only when the whole range is empty
        Diff last_chunk_len = 0;
        if (usize != 0)
        {
            last_chunk_len = static_cast<Diff>(usize % static_cast<decltype(usize)>(n));
            if (last_chunk_len == 0)
                last_chunk_len = n;
        }
        // an iterator pointing to begin of the last chunk; needed to construct common sentinel
        auto last_chunk_start = std::ranges::prev(radr::end(borrow_), last_chunk_len, radr::begin(borrow_));

        auto it  = bidi_chunk_like_iterator{borrow_, finder_};
        auto sen = bidi_chunk_like_iterator{borrow_, finder_, std::default_sentinel, last_chunk_start};

        using It  = decltype(it);
        using CIt = bidi_chunk_like_iterator<borrow_t<std::remove_cvref_t<URange> const &>, decltype(finder_)>;

        return borrowing_rad<It, It, CIt, CIt, borrowing_rad_kind::sized>{std::move(it), std::move(sen), new_size};
    }
    else // uni-directional (forward), un-common
    {
        auto finder_ = chunk_size_finder{n};

        // range is un-common → it stores the end
        auto it = unidi_chunk_like_iterator{borrow_, finder_};

        using It  = decltype(it);
        using Sen = std::conditional_t<infinite_mp_range<URange>, std::unreachable_sentinel_t, std::default_sentinel_t>;
        using CIt = unidi_chunk_like_iterator<borrow_t<std::remove_cvref_t<URange> const &>, decltype(finder_)>;
        using CSen = Sen;

        if constexpr (std::ranges::sized_range<URange>)
            return borrowing_rad<It, Sen, CIt, CSen, borrowing_rad_kind::sized>{std::move(it), Sen{}, new_size};
        else
            return borrowing_rad<It, Sen, CIt, CSen>{std::move(it), Sen{}};
    }
};

inline constexpr auto chunk_coro = []<std::ranges::input_range URange, std::integral Diff>(URange && urange, Diff n)
{
    static_assert(!container_lvalue<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    using inner_gen_t = radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>;

    return [](auto urange_, Diff n_) -> radr::generator<inner_gen_t &>
    {
        assert(n_ > 0 && "radr::chunk requires n > 0.");

        auto it = radr::begin(urange_);
        auto e  = radr::end(urange_);

        auto inner_functor = [&n_](auto & it_, auto & e_) -> inner_gen_t
        {
            for (Diff count = 0; count < n_ && it_ != e_; ++count, ++it_)
                co_yield *it_;
        };

        while (it != e)
        {
            auto tmp = inner_functor(it, e);
            co_yield tmp;
        }
    }(std::move(urange), n);
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{

/*!\brief radr::chunk(urange, n)
 * \tparam URange Type of \p urange.
 * \param urange The underlying range.
 * \param n The size of the chunks (must be > 0).
 * \details
 *
 * Turns a range into a range-of-ranges, by chunking it into consecutive, non-overlapping
 * subranges of size \p n each, except possibly the last one, which is shorter if the size
 * of \p urange is not a multiple of \p n.
 *
 * Note that there is also radr::lazy_chunk which produces the same values but weaker types.
 * See the respective documentation to decide which to use.
 *
 * ## Multi-pass ranges
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *
 * The returned "outer range"-type preserves from the underlying range:
 *   * std::ranges::random_access_range, radr::common_range, std::ranges::sized_range (all or none!)
 *   * std::ranges::bidirectional_range, radr::common_range, std::ranges::sized_range (all or none!)
 *   * else it only models std::ranges::forward_range (not common, not sized)
 *   * radr::mutable_range
 *   * radr::constant_range
 *   * radr::inifite_mp_range
 *
 * The returned "inner range" (the chunk) is created via the radr::subborrow customisation point.
 * Unless customised otherwise, it always models:
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::common_range
 *
 * It preserves from the underlying range:
 *   * categories up to std::ranges::contiguous_range
 *   * radr::mutable_range
 *   * radr::constant_range
 *
 * Construction of the adaptor is in O(n), because the first inner range is searched on construction.
 *
 * ### Notable differences to std::views::chunk
 *
 * In contrast to std::views::chunk, we do not support categories stronger than forward_range for the "outer range"-type
 * unless the underlying range is also common and sized. The "inner range"-type still preserves them, though.
 *
 * For underlying ranges that are random-access, common and sized, this adaptor is measurably faster than
 * std::views::chunk and its iterators are smaller.
 *
 * ## Single-pass ranges
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *
 * Both, the "outer range"-type and the "inner range"-type are a radr::generator.
 * This design is fully lazy; no read-ahead happens.
 *
 */
inline constexpr auto chunk = detail::pipe_with_args_fn{detail::chunk_coro, detail::chunk_borrow};

} // namespace cpo
} // namespace radr
