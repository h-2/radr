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

#include "../custom/subborrow.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../range_access.hpp"
#include "../standalone/single.hpp"

namespace radr::detail
{
template <const_borrowable_range Borrow, const_borrowable_range Pattern>
    requires std::indirectly_comparable<std::ranges::iterator_t<Borrow>,
                                        std::ranges::iterator_t<Pattern>,
                                        std::ranges::equal_to> &&
             std::semiregular<Pattern> && std::is_nothrow_default_constructible_v<Pattern> &&
             std::is_nothrow_copy_constructible_v<Pattern>
struct split_rad_iterator
{
private:
    using UIt       = iterator_t<Borrow>;
    using USen      = sentinel_t<Borrow>;
    using UCIt      = const_iterator_t<Borrow>;
    using Subborrow = subborrow_t<Borrow, UIt, UIt>;

    [[no_unique_address]] UIt       it;            // begin of current subrange
    [[no_unique_address]] USen      uend{};        // end of the underlying range
    [[no_unique_address]] Subborrow pattern_match; // [end_of_current_subrange, begin_of_pattern_matchsubrange]
    [[no_unique_address]] Pattern   pattern;       // TODO
    [[no_unique_address]] bool      trailing_empty_ = false;

    constexpr void go_next()
    {
        auto [begin, end] = std::ranges::search(std::ranges::subrange(it, uend), pattern);
        if (begin != uend && std::ranges::empty(pattern))
        {
            ++begin;
            ++end;
        }

        pattern_match = subborrow(Borrow{}, begin, end);
    }

public:
    /*!\name Associated types
     * \{
     */
    using split_rad_iterator_concept  = std::forward_iterator_tag;
    using split_rad_iterator_category = std::input_iterator_tag;
    using value_type                  = Subborrow;
    using difference_type             = std::ranges::range_difference_t<value_type>;
    //!\}

    /*!\name Constructors, destructor and assignments.
     * \{
     */
    constexpr split_rad_iterator()                                       = default;
    constexpr split_rad_iterator(split_rad_iterator const &)             = default;
    constexpr split_rad_iterator(split_rad_iterator &&)                  = default;
    constexpr split_rad_iterator & operator=(split_rad_iterator const &) = default;
    constexpr split_rad_iterator & operator=(split_rad_iterator &&)      = default;

    //!\brief Construct from values.
    constexpr split_rad_iterator(Borrow urange_, Pattern pattern_) :
      it{radr::begin(urange_)}, uend{radr::end(urange_)}, pattern{pattern_}
    {
        go_next();
    }

    //!\brief Construct from compatible iterator, in particular non-const to const.
    template <different_from<Borrow> Borrow2, typename Pattern2>
        requires(std::constructible_from<UIt, typename split_rad_iterator<Borrow2, Pattern2>::UIt> &&
                 std::constructible_from<USen, typename split_rad_iterator<Borrow2, Pattern2>::USen> &&
                 std::constructible_from<Subborrow,
                                         iterator_t<typename split_rad_iterator<Borrow2, Pattern2>::Subborrow>,
                                         sentinel_t<typename split_rad_iterator<Borrow2, Pattern2>::Subborrow>> &&
                 std::constructible_from<Pattern, typename split_rad_iterator<Borrow2, Pattern2>::USen>)
    constexpr split_rad_iterator(split_rad_iterator<Borrow2, Pattern2> citer) :
      it{std::move(citer.it)},
      uend{std::move(citer.uend)},
      pattern_match{radr::begin(citer.pattern_match), radr::end(citer.pattern_match)},
      pattern{std::move(citer.pattern_)}
    {}
    //!\}

    /*!\name Iterator operators
     * \{
     */
    constexpr value_type operator*() const { return subborrow(Borrow{}, it, pattern_match.begin()); }

    constexpr split_rad_iterator & operator++()
    {
        it = pattern_match.begin();
        if (it != uend)
        {
            it = pattern_match.end();
            if (it == uend)
            {
                trailing_empty_ = true;
                pattern_match   = subborrow(Borrow{}, it, it);
            }
            else
            {
                go_next();
            }
        }
        else
        {
            trailing_empty_ = false;
        }
        return *this;
    }

    constexpr split_rad_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    friend constexpr bool operator==(split_rad_iterator const & x, split_rad_iterator const & y)
    {
        return x.it == y.it && x.trailing_empty_ == y.trailing_empty_;
    }

    friend constexpr bool operator==(split_rad_iterator const & x, USen const & y)
    {
        return x.it == y && !x.trailing_empty_;
    }
    //!\}
};

inline constexpr auto split_borrow_impl =
  []<const_borrowable_range URange, const_borrowable_range Pattern>(URange && urange, Pattern && pattern)
    requires std::
      indirectly_comparable<std::ranges::iterator_t<URange>, std::ranges::iterator_t<Pattern>, std::ranges::equal_to>
{
    auto borrow_  = radr::borrow(urange);
    auto pattern_ = radr::borrow(pattern);

    auto it  = split_rad_iterator{borrow_, pattern_};
    auto sen = radr::end(borrow_); // use the underlying range's sentinel as-is

    using It   = decltype(it);
    using Sen  = sentinel_t<URange>;
    using CIt  = split_rad_iterator<borrow_t<URange const &>, borrow_t<Pattern>>;
    using CSen = const_sentinel_t<URange>;

    return borrowing_rad<It, Sen, CIt, CSen>{std::move(it), std::move(sen)};
};

template <typename URange, typename Pattern>
concept comparable_ranges =
  std::ranges::forward_range<URange> && std::ranges::forward_range<Pattern> &&
  std::indirectly_comparable<std::ranges::iterator_t<URange>, std::ranges::iterator_t<Pattern>, std::ranges::equal_to>;

// clang-format off
inline constexpr auto split_borrow = overloaded{
/* split by lvalue range */
[]<const_borrowable_range URange, typename Pattern>(URange && urange, std::reference_wrapper<Pattern> const & val)
    requires comparable_ranges<URange, Pattern>
{
    return split_borrow_impl(std::forward<URange>(urange), static_cast<Pattern &>(val));
},
/* split by rvalue range */
[]<const_borrowable_range URange, typename Pattern>(URange && urange, Pattern const & val)
    requires comparable_ranges<URange, Pattern>
{
    static_assert(const_borrowable_range<std::remove_cvref_t<Pattern>>,
                  "The Pattern must be a const-iterable, borrowed range. "
                  "Did you forgot to wrap it in std::ref or std::cref?");
    return split_borrow_impl(std::forward<URange>(urange), val);
},
/* split by lvalue element */
[]<const_borrowable_range URange, typename Pattern>(URange &&                               urange,
                                                    std::reference_wrapper<Pattern> const & val)
    requires(!comparable_ranges<URange, Pattern> &&
             std::equality_comparable_with<std::ranges::range_reference_t<URange>, Pattern>)
{
    return split_borrow_impl(std::forward<URange>(urange), single<Pattern, single_storage::indirect>(val));
},
/* split by rvalue element */
[]<const_borrowable_range URange, typename Pattern>(URange && urange, Pattern const & val)
    requires(!comparable_ranges<URange, Pattern> &&
             std::equality_comparable_with<std::ranges::range_reference_t<URange>, Pattern>)
{
    static_assert(std::is_nothrow_default_constructible_v<Pattern> && std::is_nothrow_copy_constructible_v<Pattern>,
                  "The value type needs to be nothrow_default_constructible and nothrow_copy_constructible for "
                  "in_iterator storage.");
    return split_borrow_impl(std::forward<URange>(urange), single<Pattern, single_storage::in_iterator>(val));
}};
// clang-format on

inline constexpr auto split_coro = []<movable_range URange, typename Pattern>(URange && urange, Pattern pattern)
    requires std::equality_comparable_with<std::ranges::range_reference_t<URange>, Pattern>
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);

    using inner_gen_t = radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>;

    return [](auto urange_, Pattern pattern_) -> radr::generator<inner_gen_t &>
    {
        auto it = radr::begin(urange_);
        auto e  = radr::end(urange_);

        bool trailing_empty = false;

        auto inner_functor = [&pattern_, &trailing_empty](auto & it_, auto & e_) -> inner_gen_t
        {
            while (it_ != e_)
            {
                if (*it_ == pattern_)
                {
                    ++it_;
                    if (it_ == e_)
                        trailing_empty = true;
                    co_return;
                }
                else
                {
                    co_yield *it_;
                    ++it_;
                }
            }
        };

        while (it != e)
        {
            auto tmp = inner_functor(it, e);
            co_yield tmp;
        }

        if (trailing_empty)
        {
            auto tmp = inner_functor(it, e);
            co_yield tmp;
        }
    }(std::move(urange), std::move(pattern));
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{

/*!\brief radr::split(urange, pattern)
 * \param urange The underlying range.
 * \param pattern The pattern to split by.
 * \details
 *
 * ## Multi-pass ranges
 *
 * The pattern can be one of:
 *   * a std::ref-wrapped lvalue of a range whose elements are comparable to the underlying range.
 *   * an rvalue of a range whose elements are comparable to the underlying range (only if the pattern is a const_borrowable_range).
 *   * a std::ref-wrapped lvalue of a delimiter element that is comparable to elements of the underlying range.
 *   * an rvalue of a delimiter element that is comparable to elements of the underlying range (only if that value is default constructible and copyable without throwing).
 *
 * Note that string-literal patterns (`"foobar"`) are not supported. Use string_views instead (`"foobar"sv`).
 *
 * ## Single-pass ranges
 *
 * The pattern can be delimiter element (rvalue or lvalue) comparable to the elements of the underlying range.
 *
 */
inline constexpr auto split = detail::pipe_with_args_fn{detail::split_coro, detail::split_borrow};
} // namespace cpo
} // namespace radr
