// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 The LLVM Project
// Copyright (c) 2023-2025 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>
#include <functional>
#include <iterator>
#include <ranges>

#include "../concepts.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../detail/semiregular_box.hpp"
#include "../generator.hpp"
#include "../range_access.hpp"

namespace radr::detail
{

template <typename Func1, typename Func2>
struct and_fn
{
    [[no_unique_address]] Func1 func1;
    [[no_unique_address]] Func2 func2;

    constexpr bool operator()(auto && val) const noexcept(noexcept(func1(val)) && noexcept(func2(val)))
    {
        return func1(val) && func2(val);
    }
};

template <typename Func, typename Iter>
concept filter_func_constraints =
  std::is_object_v<Func> && std::copy_constructible<Func> && std::indirect_unary_predicate<Func const, Iter>;

template <std::forward_iterator Iter, std::sentinel_for<Iter> Sent, filter_func_constraints<Iter> Func>
class filter_iterator
{
    [[no_unique_address]] semiregular_box<Func> func_;
    [[no_unique_address]] Iter                  current_{};
    [[no_unique_address]] Sent                  end_{};

    template <typename Container>
    constexpr friend auto tag_invoke(custom::rebind_iterator_tag,
                                     filter_iterator it,
                                     Container &     container_old,
                                     Container &     container_new)
    {
        it.current_ = tag_invoke(custom::rebind_iterator_tag{}, it.current_, container_old, container_new);
        it.end_     = tag_invoke(custom::rebind_iterator_tag{}, it.end_, container_old, container_new);
        return it;
    }

    template <typename Size>
    static constexpr auto subborrow_impl(filter_iterator it, filter_iterator sen, Size s)
    {
        using RIt     = filter_iterator<Iter, Iter, Func>;
        Iter new_uend = sen.base_iter();

        if constexpr (std::bidirectional_iterator<Iter> && !std::same_as<Iter, Sent>)
        {
            if (it != sen)
            {
                /* For filter iterators on non-common ranges, sen might not represent the "real"
                 * underlying end, because the "caching mechanism" implemented in operator++
                 * does not take effect.
                 * For these ranges, we do the tiny hack of creating a "filter_iterator-on-common",
                 * and then we decrement which will move onto the last valid underlying element.
                 * The we increment that to get the real underlying past-the-end iterator.
                 *
                 * A cleaner solution would be to introduce a `to_common` customisation point.
                 * This would allow avoiding the back and forth.
                 * We might do this in the future!
                 */
                RIt rsen{sen.func(), new_uend, new_uend};
                --rsen;
                new_uend = std::move(rsen).base_iter();
                ++new_uend;
            }
        }

        RIt rit{std::move(it).func(), std::move(it).base_iter(), new_uend};
        RIt rsen{std::move(sen).func(), new_uend, new_uend};
        return borrowing_rad{std::move(rit), std::move(rsen), s};
    }

public:
    using iterator_concept =
      std::conditional_t<std::bidirectional_iterator<Iter>, std::bidirectional_iterator_tag, std::forward_iterator_tag>;

    using iterator_category = std::
      conditional_t<std::is_lvalue_reference_v<std::iter_reference_t<Iter>>, iterator_concept, std::input_iterator_tag>;
    using value_type      = std::iter_value_t<Iter>;
    using difference_type = std::iter_difference_t<Iter>;

    filter_iterator() = default;

    constexpr filter_iterator(Func func, Iter current, Sent end) :
      func_(std::in_place, std::move(func)), current_(std::move(current)), end_(std::move(end))
    {}

    constexpr Iter const & base_iter() const & noexcept { return current_; }
    constexpr Iter         base_iter() && { return std::move(current_); }

    constexpr Sent const & base_sent() const & noexcept { return end_; }
    constexpr Sent         base_sent() && { return std::move(end_); }

    constexpr Func const & func() const & noexcept { return *func_; }
    constexpr Func         func() && { return std::move(*func_); }

    constexpr std::iter_reference_t<Iter> operator*() const { return *current_; }
    constexpr Iter                        operator->() const
        requires has_arrow<Iter>
    {
        return current_;
    }

    constexpr filter_iterator & operator++()
    {
        if constexpr (std::same_as<Iter, Sent>)
        {
            if (auto tmp = std::ranges::find_if(++current_, end_, std::ref(*func_)); tmp != end_)
                current_ = std::move(tmp);
            else // cache the real underlying end
                end_ = current_;
        }
        else
        {
            current_ = std::ranges::find_if(std::move(++current_), end_, std::ref(*func_));
        }
        return *this;
    }

    constexpr filter_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr filter_iterator & operator--()
        requires std::bidirectional_iterator<Iter>
    {
        do
        {
            --current_;
        }
        while (!std::invoke(*func_, *current_));
        return *this;
    }
    constexpr filter_iterator operator--(int)
        requires std::bidirectional_iterator<Iter>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    friend constexpr bool operator==(filter_iterator const & x, filter_iterator const & y)
    {
        return x.current_ == y.current_;
    }

    friend constexpr bool operator==(filter_iterator const & x, std::default_sentinel_t const &)
    {
        return x.current_ == x.end_;
    }

    friend constexpr std::iter_rvalue_reference_t<Iter> iter_move(filter_iterator const & it) noexcept(
      noexcept(std::ranges::iter_move(it.current_)))
    {
        return std::ranges::iter_move(it.current_);
    }

    friend constexpr void iter_swap(filter_iterator const & x, filter_iterator const & y) noexcept(
      noexcept(std::ranges::iter_swap(x.current_, y.current_)))
        requires std::indirectly_swappable<Iter>
    {
        std::ranges::iter_swap(x.current_, y.current_);
    }

    /*!\brief Customisation for subranges.
     * \details
     *
     * This potentially returns a range of different filter_iterators. In particular, the returned
     * ones are always built on iterator-sentinel of the same type.
     */
    template <borrowed_mp_range R>
    constexpr friend auto tag_invoke(custom::subborrow_tag, R &&, filter_iterator it, filter_iterator sen)
    {
        return subborrow_impl(it, sen, not_size{});
    }

    //!\overload
    template <borrowed_mp_range R>
    constexpr friend auto tag_invoke(custom::subborrow_tag,
                                     R &&,
                                     filter_iterator it,
                                     filter_iterator sen,
                                     size_t const    s)
    {
        return subborrow_impl(it, sen, s);
    }
};

// iterator-based borrow
inline constexpr auto filter_borrow_impl =
  []<typename UCIt, typename UCSen, typename Fn_>(UCIt it, UCSen sen, Fn_ _fn) // generic case
{
    using CIt  = filter_iterator<UCIt, UCSen, Fn_>;
    using CSen = std::default_sentinel_t;

    using BorrowingRad = borrowing_rad<CIt, CSen, CIt, CSen, borrowing_rad_kind::unsized>;

    // eagerly search for begin
    auto begin = std::ranges::find_if(it, sen, std::ref(_fn));

    return BorrowingRad{
      CIt{_fn, std::move(begin), std::move(sen)},
      std::default_sentinel
    };
};

inline constexpr auto filter_borrow =
  []<borrowed_mp_range URange, filter_func_constraints<radr::const_iterator_t<URange>> Fn>(URange && urange, Fn fn)
{
    // dispatch between generic case and chained case(s)
    return overloaded{
      filter_borrow_impl,
      []<typename UUCIt, typename UUCSen, typename UFn, typename Fn_>(filter_iterator<UUCIt, UUCSen, UFn> citer,
                                                                      std::default_sentinel_t,
                                                                      Fn_ new_fn)
    {
        return filter_borrow_impl(std::move(citer).base_iter(),
                                  std::move(citer).base_sent(),
                                  and_fn{std::move(citer).func(), std::move(new_fn)});
    }}(radr::cbegin(urange), radr::cend(urange), std::move(fn));
};

inline constexpr auto filter_coro =
  []<std::ranges::input_range URange, std::indirectly_unary_invocable<std::ranges::iterator_t<URange>> Fn>(
    URange && urange,
    Fn        fn)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    // we need to create inner functor so that it can take by value
    return [](auto urange_,
              Fn   fn_) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        for (auto && elem : urange_)
            if (fn_(elem))
                co_yield elem;
    }(std::move(urange), std::move(fn));
};

} // namespace radr::detail

namespace radr
{
inline namespace cpo
{
/*!\brief Take only those elements from the underlying range that satisfy a predicate.
 * \param[in] urange The underlying range.
 * \param[in] predicate The predicate.
 *
 * \details
 *
 * ## Multi-pass ranges
 *
 * * Requirements on \p urange : radr::mp_range
 * * Requirements on \p predicate : std::copy_constructible, std::is_object_v, std::indirect_unary_predicate (const predicate with urange's const iterator)
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::bidirectional_range
 *   * std::ranges::borrowed_range
 *   * radr::constant_range
 *
 * It does not preserve:
 *  * std::ranges::sized_range
 *
 * **In contrast to std::views::filter, it DOES NOT PRESERVE:**
 *   * radr::common_range
 *   * radr::mutable_range
 *
 * To prevent UB, the returned range is always a radr::constant_range.
 *
 * Use `radr::filter(Fn) | radr::to_common` to make the range common, and use
 * `radr::to_single_pass | radr::filter(Fn)`, create a mutable range (see below).
 *
 * Construction of the adaptor is in O(n), because the first matching element is searched and cached.
 *
 * Multiple nested filter adaptors are folded into one.
 *
 * ## Single-pass ranges
 *
 * * Requirements on \p urange : std::ranges::input_range
 * * Requirements on \p predicate : std::move_constructible, std::is_object_v, std::indirect_unary_invocable (mutable predicate with urange's non-const iterator)
 *
 * The single-pass version of this adaptor preserves mutability, i.e. it allows changes to the underlying range's elements.
 * It also allows (observable) changes in the predicate.
 *
 */
inline constexpr auto filter = detail::pipe_with_args_fn{detail::filter_coro, detail::filter_borrow};
} // namespace cpo
} // namespace radr
