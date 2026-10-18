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

template <typename Func, typename It>
concept filter_func_constraints = object<Func> && std::indirect_unary_predicate<Func const, It>;

template <std::forward_iterator UIt, std::sentinel_for<UIt> USen, filter_func_constraints<UIt> Func>
class filter_iterator
{
    [[no_unique_address]] semiregular_box<Func> fn;
    [[no_unique_address]] UIt                   uit{};
    [[no_unique_address]] USen                  uend{};

    template <typename Container>
    constexpr friend auto tag_invoke(custom::rebind_iterator_tag,
                                     filter_iterator it,
                                     Container &     container_old,
                                     Container &     container_new)
    {
        it.uit  = tag_invoke(custom::rebind_iterator_tag{}, it.uit, container_old, container_new);
        it.uend = tag_invoke(custom::rebind_iterator_tag{}, it.uend, container_old, container_new);
        return it;
    }

    template <typename Size>
    static constexpr auto subborrow_impl(filter_iterator it, filter_iterator sen, Size s)
    {
        using RIt    = filter_iterator<UIt, UIt, Func>;
        UIt new_uend = sen.base_iter();

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
        UIt  ubeg = std::move(it).base_iter();
        USen uend = std::move(it).base_sent();
        Func fn   = std::move(it).func();

        UIt it_end{};

        /* search backwards from end if possible */
        if constexpr (std::bidirectional_iterator<UIt> && std::same_as<UIt, USen>)
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
      std::conditional_t<std::bidirectional_iterator<UIt>, std::bidirectional_iterator_tag, std::forward_iterator_tag>;

    using iterator_category = std::
      conditional_t<std::is_lvalue_reference_v<std::iter_reference_t<UIt>>, iterator_concept, std::input_iterator_tag>;
    using value_type      = std::iter_value_t<UIt>;
    using difference_type = std::iter_difference_t<UIt>;

    filter_iterator() = default;

    constexpr filter_iterator(Func func, UIt current, USen end) :
      fn(std::in_place, std::move(func)), uit(std::move(current)), uend(std::move(end))
    {}

    constexpr UIt const & base_iter() const & noexcept { return uit; }
    constexpr UIt         base_iter() && { return std::move(uit); }

    constexpr USen const & base_sent() const & noexcept { return uend; }
    constexpr USen         base_sent() && { return std::move(uend); }

    constexpr Func const & func() const & noexcept { return *fn; }
    constexpr Func         func() && { return std::move(*fn); }

    constexpr std::iter_reference_t<UIt> operator*() const { return *uit; }
    constexpr UIt                        operator->() const
        requires has_arrow<UIt>
    {
        return uit;
    }

    constexpr filter_iterator & operator++()
    {
        /* The formulation of this loop has a large effect on the generated code (measured on Zen+ with GCC 13 and
         * clang 20, see the filter benchmarks before changing it):
         *  - std::ranges::find_if(++uit, uend, *fn): GCC (12–16) cross-jumps that loop's latch with the caller's
         *    "++it; it != end", which doubles the taken branches per element (up to 1.5x slower than
         *    std::views::filter). `for (++uit; uit != uend; ++uit) if (pred) break;` has the same problem on lists.
         *  - do { ++uit; } while (uit != uend && !pred): avoids that, but is slower for folded filters (GCC) and
         *    places the inner loop's alignment padding on the per-element path (clang).
         *  - this loop without [[likely]]: same code as the do-while loop (clang); slower than the do-while loop
         *    for predicates that keep most elements (GCC).
         * [[likely]] only changes the block layout (no hardware hint is emitted on x86); the dynamic branch
         * prediction is not affected. It was also measured with predicates that reject most elements and did not
         * slow these down on average (GCC: faster; clang: equal, except up to 1.1x slower on lists).
         * Only measured on Zen+; results on other microarchitectures may differ. */
        while (++uit != uend)
            if (std::invoke(*fn, *uit)) [[likely]]
                return *this;

        return *this;
    }

    constexpr filter_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr filter_iterator & operator--()
        requires std::bidirectional_iterator<UIt>
    {
        do
        {
            --uit;
        }
        while (!std::invoke(*fn, *uit));
        return *this;
    }
    constexpr filter_iterator operator--(int)
        requires std::bidirectional_iterator<UIt>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    friend constexpr bool operator==(filter_iterator const & x, filter_iterator const & y) { return x.uit == y.uit; }

    friend constexpr bool operator==(filter_iterator const & x, std::default_sentinel_t const &)
    {
        return x.uit == x.uend;
    }

    friend constexpr std::iter_rvalue_reference_t<UIt> iter_move(filter_iterator const & it) noexcept(
      noexcept(std::ranges::iter_move(it.uit)))
    {
        return std::ranges::iter_move(it.uit);
    }

    friend constexpr void iter_swap(filter_iterator const & x,
                                    filter_iterator const & y) noexcept(noexcept(std::ranges::iter_swap(x.uit, y.uit)))
        requires std::indirectly_swappable<UIt>
    {
        std::ranges::iter_swap(x.uit, y.uit);
    }
};

inline constexpr auto filter_borrow =
  []<borrowed_mp_range URange, filter_func_constraints<radr::const_iterator_t<URange>> Fn>(URange && urange, Fn fn)
{
    // iterator-based borrow
    constexpr auto filter_borrow_impl =
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

    // dispatch between generic case and chained case(s)
    return overloaded{
      filter_borrow_impl,
      []<typename UUCIt, typename UUCSen, typename UFn, typename Fn_>(filter_iterator<UUCIt, UUCSen, UFn> citer,
                                                                      std::default_sentinel_t,
                                                                      Fn_ new_fn)
    {
        return decltype(filter_borrow_impl){}(std::move(citer).base_iter(),
                                              std::move(citer).base_sent(),
                                              and_fn{std::move(citer).func(), std::move(new_fn)});
    }}(radr::cbegin(urange), radr::cend(urange), std::move(fn));
};

inline constexpr auto filter_coro = []<std::ranges::input_range URange, typename Fn>(URange && urange, Fn fn)
{
    static_assert(!container_lvalue<URange>, RADR_ASSERTSTRING_RVALUE);
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
 * To prevent UB, the returned range is always a radr::constant_range, i.e. you cannot change the underlying range's
 * elements.
 *
 * Construction of the adaptor is in O(n), because the first matching element is searched and cached, and the range
 * might not contain any matching elements.
 *
 * Multiple chained filter adaptors are folded into one.
 *
 * ### Notable differences to std::views::filter
 *
 * Like all our multi-pass adaptors (but unlike std::views::filter), this adaptor is const-iterable.
 *
 * Does not preserve radr::common_range. Use `… | radr::filter(fn) | radr::to_common` if you want a common range.
 * This has the added benefit that the real underlying end is used and the tail of the underlying range is not searched
 * repeatedly. See [caching end](docs/caching_begin.md#caching-end) for more details.
 *
 * Does not preserve radr::mutable_range, i.e. it always returns radr::constant_range.
 * This prevents undefined behaviour by accidentally changing values that invalidate the predicate.
 * If you want a mutable range, use the single pass adaptor like this:
 * `… | radr::to_single_pass | radr::filter(fn)`
 *
 *
 * ### Performance
 *
 * Micro-benchmarks show that radr::filter performs better on average than std::views::filter (≥ speed, ≤ instructions),
 * although results vary between CPUs, compilers and standard libraries. In few cases it performs worse.
 *
 * Note that the folding mechanism reduces iterator size and template instantiations, but that compilers may re-order
 * the predicates which can influence performance in both directions.
 *
 * ## Single-pass ranges
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *   * `radr::weak_indirect_unary_invocable<Predicate &, radr::iterator_t<URange>>`
 *
 * The single-pass version of this adaptor preserves mutability, i.e. it allows changes to the underlying range's
 * elements. It also allows (observable) changes in the predicate.
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
