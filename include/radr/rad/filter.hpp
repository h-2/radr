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
#include "radr/custom/tags.hpp"

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
concept filter_func_constraints = object<Func> && std::indirect_unary_predicate<Func const, Iter>;

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

        RIt rit{std::move(it).func(), std::move(it).base_iter(), new_uend};
        RIt rsen{std::move(sen).func(), new_uend, new_uend};
        return borrowing_rad{std::move(rit), std::move(rsen), s};
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

    //!\brief Customisation to create common sentinel with actual underlying end.
    constexpr friend filter_iterator tag_invoke(custom::find_common_end_tag,
                                                filter_iterator it,
                                                std::default_sentinel_t)
    {
        Iter ubeg = std::move(it).base_iter();
        Sent uend = std::move(it).base_sent();
        Func fn   = std::move(it).func();

        Iter it_end{};

        /* search backwards from end if possible */
        if constexpr (std::bidirectional_iterator<Iter> && std::same_as<Iter, Sent>)
        {
            it_end = uend;
            while (it_end != ubeg)
            {
                --it_end;
                if (std::invoke(std::cref(fn), *it_end))
                {
                    ++it_end;
                    break;
                }
            }
        }
        /* search from beginning but store element behind last matching instead of underlying end */
        else
        {
            bool empty = true;

            for (auto it = ubeg; it != uend; ++it)
            {
                if (std::invoke(std::cref(fn), *it))
                {
                    it_end = it;
                    empty  = false;
                }
            }

            if (!empty)
                ++it_end;
        }

        return {std::move(fn), std::move(it_end), std::move(uend)};
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
        current_ = std::ranges::find_if(std::move(++current_), end_, std::cref(*func_));

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

inline constexpr auto filter_coro = []<std::ranges::input_range URange, typename Fn>(URange && urange, Fn fn)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    static_assert(weak_indirect_unary_invocable<Fn, std::ranges::iterator_t<URange>>,
                  "The constraints for radr::take_while's functor are not satisfied.");

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
 * \tparam URange Type of \p urange.
 * \tparam Predicate Type of \p predicate.
 *
 * \details
 *
 * ## Multi-pass ranges
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * `std::indirect_unary_predicate<Predicate const &, radr::iterator_t<URange>>`
 *   * `std::indirect_unary_predicate<Predicate const &, radr::const_iterator_t<URange>>`
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::bidirectional_range
 *   * std::ranges::borrowed_range
 *   * radr::constant_range
 *
 * It does not preserve:
 *   * std::ranges::sized_range
 *   * radr::common_range
 *   * radr::mutable_range
 *
 * To prevent UB, the returned range is always a radr::constant_range.
 *
 * Construction of the adaptor is in O(n), because the first matching element is searched and cached.
 *
 * Multiple nested filter adaptors are folded into one.
 *
 * ### Notable differences to std::views::filter
 *
 * Like all our multi-pass adaptors (but unlike std::views::filter), this adaptor is const-iterable.
 *
 * Does not preserve radr::common_range. Use `… | radr::filter(Fn) | radr::to_common` if you want a common range.
 * This has the added benefit that the real underlying end is used and the tail of the underlying range is not searched repeatedly.
 * See [caching end](docs/caching_begin.md#caching-end) for more details.
 *
 * Does not preserve radr::mutable_range, i.e. it always returns radr::constant_range.
 * This prevents undefined behaviour by accidentally changing values that invalidate the predicate.
 * If you want a mutable range, use the single pass adaptor like this:
 * `… | radr::to_single_pass | radr::filter(fn)`
 *
 * ## Single-pass ranges
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *   * `radr::weak_indirect_unary_invocable<Predicate, radr::iterator_t<URange>>`
 *
 * The single-pass version of this adaptor preserves mutability, i.e. it allows changes to the underlying range's elements.
 * It also allows (observable) changes in the predicate.
 *
 * ### Notable differences to std::views::filter
 *
 * In C++26 the single-pass version of std::views::filter was made const-iterable (but not the multi-pass version).
 * This is counter to any consistency in the range concepts.
 *
 * All our multi-pass adaptors are const-iterable, but non of our single-pass adaptors are.
 *
 */
inline constexpr auto filter = detail::pipe_with_args_fn{detail::filter_coro, detail::filter_borrow};
} // namespace cpo
} // namespace radr
