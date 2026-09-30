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

// ==========================================================================
// unidi_chunk_like_iterator [for radr::chunk, radr::chunk_by, radr::slide]
// ==========================================================================

namespace radr::detail
{

/*!\brief The forward-only iterator for radr::chunk, radr::chunk_by and radr::slide.
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

    //!\brief The return type determines whether subranges are sized or not.
    template <typename UIt, typename USen>
    using chunk_size_t = decltype(finder.chunk_size(std::declval<UIt const &>(),
                                                    std::declval<UIt const &>(),
                                                    std::declval<USen const &>()));

public:
    /*!\name Associated types
     * \{
     */
    using iterator_concept  = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type        = subborrow_t<Borrow, UIt, UIt, chunk_size_t<UIt, USen>>;
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
      finder{std::move(finder_)},
      uend{radr::end(urange_)},
      subrange_begin{radr::begin(urange_)},
      subrange_end{radr::begin(urange_)}
    {
        finder.init_begin(subrange_begin, subrange_end, uend);
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
        return subborrow(Borrow{}, subrange_begin, subrange_end, finder.chunk_size(subrange_begin, subrange_end, uend));
    }

    constexpr unidi_chunk_like_iterator & operator++()
    {
        finder.go_next(subrange_begin, subrange_end, uend);
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

// ==========================================================================
// bidi_chunk_like_iterator [for radr::chunk, radr::chunk_by, radr::slide]
// ==========================================================================

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

    //!\brief The return type determines whether subranges are sized or not.
    template <typename UIt, typename USen>
    using chunk_size_t = decltype(finder.chunk_size(std::declval<UIt const &>(),
                                                    std::declval<UIt const &>(),
                                                    std::declval<USen const &>()));

public:
    /*!\name Associated types
     * \{
     */
    using iterator_concept  = std::bidirectional_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type        = subborrow_t<Borrow, UIt, UIt, chunk_size_t<UIt, UIt>>;
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
      finder{std::move(finder_)},
      ubegin{radr::begin(urange_)},
      uend{radr::end(urange_)},
      subrange_begin{ubegin},
      subrange_end{ubegin}
    {
        finder.init_begin(subrange_begin, subrange_end, uend);
    }

    //!\brief Construct at the end.
    constexpr bidi_chunk_like_iterator(Borrow urange_, BoundaryFinder finder_, std::default_sentinel_t) :
      finder{std::move(finder_)},
      ubegin{radr::begin(urange_)},
      uend{radr::end(urange_)},
      subrange_begin{uend},
      subrange_end{uend}
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
        return subborrow(Borrow{}, subrange_begin, subrange_end, finder.chunk_size(subrange_begin, subrange_end, uend));
    }

    constexpr bidi_chunk_like_iterator & operator++()
    {
        finder.go_next(subrange_begin, subrange_end, uend);
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
        finder.go_prev(subrange_begin, subrange_end, ubegin, uend);
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

// ==========================================================================
// ra_chunk_like_iterator [for radr::chunk]
// ==========================================================================

/*!\brief The random-access iterator used by radr::chunk and radr::slide when the underlying range is RA+sized.
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
            // cheap pointer-arithmetic path when the jump stays within the current block), a
            // second full-magnitude advance would be measurably more expensive.
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

// ==========================================================================
// finder types for radr::chunk
// ==========================================================================

//!\brief Encodes behaviour that is specific to radr::chunk in the unidi case
template <std::integral Diff>
struct chunk_size_finder_unidi
{
    Diff n{};

    //!\brief Find first subrange.
    template <typename UIt, typename USen>
    constexpr void init_begin(UIt & subrange_begin, UIt & subrange_end, USen const uend) const
    {
        if (subrange_begin != uend)
            go_next(subrange_begin, subrange_end, uend);
    }

    //!\brief Find next subrange.
    template <typename UIt, typename USen>
    constexpr void go_next(UIt & subrange_begin, UIt & subrange_end, USen const uend) const
    {
        assert(subrange_begin != uend); // not already at end
        subrange_begin = subrange_end;
        subrange_end   = std::ranges::next(std::move(subrange_end), n, uend);
    }

    //!\brief Size of subrange.
    template <typename UIt, typename USen>
    constexpr auto chunk_size(UIt const subrange_begin, UIt const subrange_end, [[maybe_unused]] USen const uend) const
    {
        if constexpr (std::sized_sentinel_for<UIt, UIt>)
        {
            return to_unsigned_like(subrange_end - subrange_begin);
        }
        else
        {
            // only the last chunk may be shorter than n, so the linear count happens at most once per traversal
            return to_unsigned_like(subrange_end == uend ? std::ranges::distance(subrange_begin, subrange_end) : n);
        }
    }
};

//!\brief Encodes behaviour that is specific to radr::chunk in the bidi case
// (separate from above so unidi saves a member).
template <std::integral Diff>
class chunk_size_finder_bidi : protected chunk_size_finder_unidi<Diff>
{
private:
    Diff last_chunk_len = -1;

    using base_t = chunk_size_finder_unidi<Diff>;
    using base_t::n;

public:
    constexpr chunk_size_finder_bidi()                                           = default;
    constexpr chunk_size_finder_bidi(chunk_size_finder_bidi &&)                  = default;
    constexpr chunk_size_finder_bidi(chunk_size_finder_bidi const &)             = default;
    constexpr chunk_size_finder_bidi & operator=(chunk_size_finder_bidi &&)      = default;
    constexpr chunk_size_finder_bidi & operator=(chunk_size_finder_bidi const &) = default;

    template <typename URange>
    constexpr chunk_size_finder_bidi(Diff n_, URange const & urange) : chunk_size_finder_unidi<Diff>{n_}
    {
        static_assert(std::ranges::sized_range<URange>, RADR_BUG(__FILE__, __LINE__));
        static_assert(common_range<URange>, RADR_BUG(__FILE__, __LINE__));

        auto usize     = std::ranges::size(urange);
        // length of the last chunk which is potentially shorter than n
        // 0 only when the whole range is empty
        last_chunk_len = 0;
        if (usize != 0)
        {
            last_chunk_len = static_cast<Diff>(usize % static_cast<decltype(usize)>(n));
            if (last_chunk_len == 0)
                last_chunk_len = n;
        }
    }

    using base_t::go_next;
    using base_t::init_begin;

    template <std::bidirectional_iterator UIt>
    constexpr void go_prev(UIt & subrange_begin, UIt & subrange_end, UIt const & ubegin, UIt const uend) const
    {
        assert(subrange_begin != ubegin);

        subrange_end = subrange_begin;

        if (subrange_begin == uend) // "at-end"
        {
            subrange_begin = std::ranges::prev(std::move(subrange_begin), last_chunk_len, ubegin);
        }
        else
        {
            subrange_begin = std::ranges::prev(std::move(subrange_begin), n, ubegin);
        }
    }

    template <typename UIt, typename USen>
    constexpr auto chunk_size(UIt const &                 subrange_begin,
                              UIt const &                 subrange_end,
                              [[maybe_unused]] USen const uend) const
    {
        if constexpr (std::sized_sentinel_for<UIt, UIt>)
        {
            return to_unsigned_like(subrange_end - subrange_begin);
        }
        else
        {
            return to_unsigned_like(subrange_end == uend ? last_chunk_len : n);
        }
    }
};

// ==========================================================================
// chunk_borrow and chunk_coro
// ==========================================================================

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
        auto finder_ = chunk_size_finder_bidi{n, borrow_};
        auto it      = bidi_chunk_like_iterator{borrow_, finder_};
        auto sen     = bidi_chunk_like_iterator{borrow_, finder_, std::default_sentinel};

        using It  = decltype(it);
        using CIt = bidi_chunk_like_iterator<borrow_t<std::remove_cvref_t<URange> const &>, decltype(finder_)>;

        return borrowing_rad<It, It, CIt, CIt, borrowing_rad_kind::sized>{std::move(it), std::move(sen), new_size};
    }
    else // uni-directional (forward), un-common
    {
        auto finder_ = chunk_size_finder_unidi{n};

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
 * Note that the type of \p n influences the size of the iterators. It may be beneficial to use
 * int32_t or another type that is smaller than the std::range_difference_t of \p urange.
 *
 * ## Multi-pass ranges
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *
 * The returned "outer range"-type preserves from the underlying range:
 *   * std::ranges::random_access_range (only if also sized)
 *   * std::ranges::bidirectional_range (only if also common and sized)
 *   * radr::sized_range
 *   * radr::common_range (only if also sized and at least bidi)
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
