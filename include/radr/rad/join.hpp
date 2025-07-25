// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2024 The LLVM Project
// Copyright (c) 2023-2025 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <iterator>
#include <ranges>

#include "../custom/subborrow.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../range_access.hpp"

namespace radr::detail
{
template <std::forward_iterator UIt, std::sentinel_for<UIt> USen>
    requires borrowed_mp_range<std::iter_reference_t<UIt>>
class join_rad_iterator
{
private:
    using Inner    = borrow_t<std::iter_reference_t<UIt>>;
    using InnerIt  = radr::iterator_t<Inner>;
    using InnerSen = radr::sentinel_t<Inner>;

    static constexpr bool bidi =
      std::bidirectional_iterator<UIt> && std::ranges::bidirectional_range<Inner> && common_range<Inner>;

    static constexpr bool bidi_common = bidi && std::same_as<UIt, USen>;

    //!\brief If we use empty_t defined in detail/detail.hpp, it is not optimised out ¯\_(ツ)_/¯
    struct empty_t
    {
        constexpr empty_t() noexcept = default;
        constexpr empty_t(auto &&) noexcept {}
    };

    [[no_unique_address]] std::conditional_t<bidi, UIt, empty_t>     outer_begin{}; // begin of underlying outer rng
    [[no_unique_address]] UIt                                        outer_it{};    // position in underlying outer rng
    [[no_unique_address]] USen                                       outer_end{};   // end of underlying outer rng
    [[no_unique_address]] std::conditional_t<bidi, InnerIt, empty_t> inner_begin{}; // begin of underlying inner rng
    [[no_unique_address]] InnerIt                                    inner_it{};    // position in underlying inner rng
    [[no_unique_address]] InnerSen                                   inner_end{};   // end of underlying inner rng

    void update_inner()
    {
        while (outer_it != outer_end)
        {
            auto tmp    = borrow(*outer_it);
            inner_it    = radr::begin(tmp);
            inner_begin = radr::begin(tmp);
            inner_end   = radr::end(tmp);

            // skip empty inner ranges
            if (inner_it == inner_end)
                ++outer_it;
            else
                break;
        }
    }

    template <std::forward_iterator UIt2, std::sentinel_for<UIt2> USen2>
        requires borrowed_mp_range<std::iter_reference_t<UIt2>>
    friend class join_rad_iterator;

    template <typename Container>
    constexpr friend auto tag_invoke(custom::rebind_iterator_tag,
                                     join_rad_iterator it,
                                     Container &       container_old,
                                     Container &       container_new)
    {
        if (it == join_rad_iterator{})
            return it;

        auto inner_range_old = borrow(*it.outer_it);

        if constexpr (bidi)
            it.outer_begin = tag_invoke(custom::rebind_iterator_tag{}, it.outer_begin, container_old, container_new);
        it.outer_it  = tag_invoke(custom::rebind_iterator_tag{}, it.outer_it, container_old, container_new);
        it.outer_end = tag_invoke(custom::rebind_iterator_tag{}, it.outer_end, container_old, container_new);

        auto inner_range_new = borrow(*it.outer_it);
        if constexpr (bidi)
            it.inner_begin =
              tag_invoke(custom::rebind_iterator_tag{}, it.inner_begin, inner_range_old, inner_range_new);
        it.inner_it  = tag_invoke(custom::rebind_iterator_tag{}, it.inner_it, inner_range_old, inner_range_new);
        it.inner_end = tag_invoke(custom::rebind_iterator_tag{}, it.inner_end, inner_range_old, inner_range_new);
        return it;
    }

public:
    /*!\name Associated types
     * \{
     */
    using iterator_concept  = std::conditional_t<bidi, std::bidirectional_iterator_tag, std::forward_iterator_tag>;
    using iterator_category = std::conditional_t<std::is_lvalue_reference_v<std::iter_reference_t<InnerIt>>,
                                                 iterator_concept,
                                                 std::input_iterator_tag>;
    using value_type        = std::iter_value_t<InnerIt>;
    using difference_type   = std::iter_difference_t<InnerIt>;
    //!\}

    /*!\name Constructors, destructor and assignments.
     * \{
     */
    constexpr join_rad_iterator()                                      = default;
    constexpr join_rad_iterator(join_rad_iterator const &)             = default;
    constexpr join_rad_iterator(join_rad_iterator &&)                  = default;
    constexpr join_rad_iterator & operator=(join_rad_iterator const &) = default;
    constexpr join_rad_iterator & operator=(join_rad_iterator &&)      = default;

    //!\brief Construct from values.
    constexpr join_rad_iterator(UIt uit_, UIt ubeg_, USen usen_) :
      outer_begin{std::move(ubeg_)}, outer_it{std::move(uit_)}, outer_end{std::move(usen_)}
    {
        if constexpr (bidi_common)
        {
            /* | Sentinel construction of non-empty outer range |
             * When constructing this, inner_it is usually not initialised. This makes operator== more expensive.
             * For the bidi_common case, we can initialise it to make operator== cheaper
             */
            if (outer_it == outer_end && outer_begin != outer_end)
            {
                auto outer_back = outer_end;
                --outer_back;
                inner_it    = radr::end(*outer_back);
                inner_begin = inner_it; // necessary to allow proper operator-- on sentinel
            }
        }
        update_inner();
    }

    //!\brief Construct from compatible iterator, in particular non-const to const.
    template <different_from<UIt> UIt2, std::sentinel_for<UIt2> USen2>
        requires(std::constructible_from<UIt, UIt2> && std::constructible_from<USen, USen2> &&
                 std::constructible_from<InnerIt, iterator_t<borrow_t<std::iter_reference_t<UIt2>>>> &&
                 std::constructible_from<InnerSen, sentinel_t<borrow_t<std::iter_reference_t<UIt2>>>> &&
                 (!bidi || join_rad_iterator<UIt2, USen2>::bidi))
    constexpr join_rad_iterator(join_rad_iterator<UIt2, USen2> mutiter) :
      outer_begin{std::move(mutiter.outer_begin)},
      outer_it{std::move(mutiter.outer_it)},
      outer_end{std::move(mutiter.outer_end)},
      inner_begin{std::move(mutiter.inner_begin)},
      inner_it{std::move(mutiter.inner_it)},
      inner_end{std::move(mutiter.inner_end)}
    {}
    //!\}

    /*!\name Iterator operators
     * \{
     */
    constexpr decltype(auto) operator*() const { return *inner_it; }

    constexpr join_rad_iterator & operator++()
    {
        assert(inner_it != inner_end);
        ++inner_it;

        if (inner_it == inner_end)
        {
            assert(outer_it != outer_end);
            ++outer_it;
            update_inner();
        }
        return *this;
    }

    constexpr join_rad_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr join_rad_iterator & operator--()
        requires bidi
    {
        if (inner_it == inner_begin)
        {
            assert(outer_it != outer_begin);
            --outer_it;

            while (outer_it != outer_begin)
            {
                auto tmp = borrow(*outer_it);
                inner_it = radr::end(tmp);
                --inner_it;
                inner_begin = radr::begin(tmp);
                inner_end   = radr::end(tmp);

                // skip empty inner ranges
                if (inner_it == inner_end)
                    --outer_it;
                else
                    break;
            }
        }
        else
        {
            --inner_it;
        }
        return *this;
    }

    constexpr join_rad_iterator operator--(int)
        requires bidi
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    friend bool operator==(join_rad_iterator const & lhs, join_rad_iterator const & rhs)
    {
        if constexpr (bidi_common)
            return lhs.outer_it == rhs.outer_it && lhs.inner_it == rhs.inner_it;
        else // inner_it of sentinel may not be initialised
            return lhs.outer_it == rhs.outer_it && (lhs.outer_it == lhs.outer_end || lhs.inner_it == rhs.inner_it);
    }

    friend bool operator==(join_rad_iterator const & lhs, std::default_sentinel_t)
    {
        return lhs.outer_it == lhs.outer_end;
    }
    //!\}

    friend constexpr decltype(auto) iter_move(join_rad_iterator const & i) noexcept(
      noexcept(std::ranges::iter_move(i.inner_it)))
    {
        return std::ranges::iter_move(i.inner_it);
    }

    friend constexpr void iter_swap(join_rad_iterator const & x, join_rad_iterator const & y) noexcept(
      noexcept(std::ranges::iter_swap(x.inner_it, y.inner_it)))
        requires std::indirectly_swappable<InnerIt>
    {
        std::ranges::iter_swap(x.inner_it, y.inner_it);
    }
};

inline constexpr auto join_borrow = []<borrowed_mp_range URange>(URange && urange)
    requires borrowed_mp_range<std::ranges::range_reference_t<URange>> &&
             std::semiregular<borrow_t<std::ranges::range_reference_t<URange>>>
{
    using It   = join_rad_iterator<iterator_t<URange>, sentinel_t<URange>>;
    using Sen  = std::conditional_t<common_range<URange>, It, std::default_sentinel_t>;
    using CIt  = join_rad_iterator<const_iterator_t<URange>, const_sentinel_t<URange>>;
    using CSen = std::conditional_t<common_range<URange const>, CIt, std::default_sentinel_t>;

    //TODO only preserve common if underlying is bidi; this will make unidirectional iteration faster
    if constexpr (common_range<URange>)
        return borrowing_rad<It, Sen, CIt, CSen>{
          It{radr::begin(urange), radr::begin(urange), radr::end(urange)},
          It{  radr::end(urange), radr::begin(urange), radr::end(urange)}
        };
    else
        return borrowing_rad<It, Sen, CIt, CSen>{
          It{radr::begin(urange), radr::begin(urange), radr::end(urange)},
          std::default_sentinel
        };
};

inline constexpr auto join_coro = []<std::ranges::input_range URange>(URange && urange)
    requires std::ranges::input_range<std::ranges::range_reference_t<URange>>
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    using Inner    = std::ranges::range_reference_t<URange>;
    using InnerRef = std::ranges::range_reference_t<Inner>;
    using InnerVal = std::ranges::range_value_t<Inner>;

    return [](auto urange_) -> radr::generator<InnerRef, InnerVal>
    {
        for (auto && inner : urange_)
            for (auto && elem : inner)
                co_yield elem;
    }(std::move(urange));
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{

/*!\brief Flattens a range-of-ranges into a range.
 * \param urange The underlying range.
 *
 * ### Multi-pass adaptor
 *
 *  * Requirements on \p urange ("outer range type"): radr::mp_range
 *  * Requirements on \p urange 's `range_reference_t` ("inner range type"): radr::borrowed_mp_range
 *
 * The multi-pass range adaptor models std::ranges::bidirectional_range iff:
 *   * \p urange models std::ranges:bidirectional_range.
 *   * \p urange 's `range_reference_t` models std::ranges:bidirectional_range and radr::common_range.
 *   * These requirements are less strict than for std::views::join.
 *
 * The following concepts are preserved from \p urange :
 *   * radr::common_range
 *
 * The following concepts mirror those of the range_reference_t of \p urange :
 *   * radr::constant_range
 *   * radr::mutable_range (see below)
 *
 * This range adaptor never models std::ranges::sized_range or radr::approximately_sized_range.
 *
 * Note that the bidirectional adaptor's iterator is larger than that of the forward-only version (6 stored iterators VS
 * 4 stored iterators), so it may be beneficial to prefix the invocation like this
 * `… | radr::to_uni | radr::join | …` if you don't need bidirectionality.
 *
 * ### Single-pass adaptor
 *
 *  * Requirements on \p urange : std::ranges::input_range
 *  * Requirements on \p urange 's `range_reference_t`: std::ranges::input_range
 *
 * It is not required that the `range_reference_t` of \p urange be a `borrowed_range`.
 *
 */
inline constexpr auto join = detail::pipe_without_args_fn{detail::join_coro, detail::join_borrow};
} // namespace cpo
} // namespace radr
