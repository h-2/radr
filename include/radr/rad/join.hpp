// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2024 The LLVM Project
// Copyright (c) 2024 Hannes Hauswedell
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
    requires const_borrowable_range<std::iter_reference_t<UIt>>
class join_rad_iterator
{
private:
    using Inner    = borrow_t<std::iter_reference_t<UIt>>;
    using InnerIt  = radr::iterator_t<Inner>;
    using InnerSen = radr::sentinel_t<Inner>;

    static constexpr bool bidi =
      std::bidirectional_iterator<UIt> && std::ranges::bidirectional_range<Inner> && common_range<Inner>;

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
        requires const_borrowable_range<std::iter_reference_t<UIt2>>
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
                                                 std::forward_iterator_tag>;
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
        // The center line check prevents us from looking at inner if either of the sides is at end
        // which would make inner invalid
        if constexpr (bidi)
        {
            return std::tie(lhs.outer_it, lhs.outer_end, lhs.outer_begin) ==
                     std::tie(rhs.outer_it, rhs.outer_end, rhs.outer_begin) &&
                   ((lhs.outer_it == lhs.outer_end) || // both are at end
                    std::tie(lhs.inner_it, lhs.inner_end, lhs.inner_begin) ==
                      std::tie(rhs.inner_it, rhs.inner_end, rhs.inner_begin));
        }
        else
        {
            return std::tie(lhs.outer_it, lhs.outer_end) == std::tie(rhs.outer_it, rhs.outer_end) &&
                   ((lhs.outer_it == lhs.outer_end) || // both are at end
                    std::tie(lhs.inner_it, lhs.inner_end) == std::tie(rhs.inner_it, rhs.inner_end));
        }
    }

    friend bool operator==(join_rad_iterator const & lhs, std::default_sentinel_t)
    {
        return lhs.outer_it == lhs.outer_end;
    }
    //!\}
};

inline constexpr auto join_borrow = []<const_borrowable_range URange>(URange && urange)
    requires const_borrowable_range<std::ranges::range_reference_t<URange>> &&
             std::semiregular<borrow_t<std::ranges::range_reference_t<URange>>>
{
    using It   = join_rad_iterator<iterator_t<URange>, sentinel_t<URange>>;
    using Sen  = std::conditional_t<common_range<URange>, It, std::default_sentinel_t>;
    using CIt  = join_rad_iterator<const_iterator_t<URange>, const_sentinel_t<URange>>;
    using CSen = std::conditional_t<common_range<URange const>, CIt, std::default_sentinel_t>;

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

/*!\brief Flattens a range-of-range into a range.
 * \param urange The underlying range.
 *
 * ### Multi-pass adaptor
 *
 * The multi-pass range adaptor is a `bidirectional_range` if:
 *   * The underlying range is a `bidirectional_range`.
 *   * The `range_reference_t` of the underlying range is a `const_borrowable_range`.
 *   * The `range_reference_t` of the underlying range is a `bidirectional_range` and a `common_range`.
 * Otherwise, the multi-pass range adaptor is a forward_range if:
 *   * The underlying range is a forward_range.
 *   * The `range_reference_t` of the underlying range is a `const_borrowable_range`.
 *   * The `range_reference_t` of the underlying range is a `forward_range`.
 * Otherwise, the multi-pass range adaptor is ill-formed.
 *
 * The multi-pass range adaptor is a `common_range` iff the underlying range is a `common_range`.
 *
 * Note that the bidirectional adaptor's iterator is larger than that of the forward version (6 stored iterators VS
 * 4 stored iterators).
 *
 * ### Single-pass adaptor
 *
 * The single-pass range adaptor is well-formed iff:
 *  * The underlying range is an `input_range`.
 *  * The `range_reference_t` of the underlying range is an `input_range`.
 *
 * It is not required that the `range_reference_t` of the underlying range be a `borrowed_range`.
 *
 */
inline constexpr auto join = detail::pipe_without_args_fn{detail::join_coro, detail::join_borrow};
} // namespace cpo
} // namespace radr
