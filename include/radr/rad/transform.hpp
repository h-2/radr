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
#include <concepts>
#include <functional>
#include <iterator>
#include <ranges>

#include "../concepts.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../detail/semiregular_box.hpp"
#include "../generator.hpp"
#include "radr/range_access.hpp"

namespace radr::detail::transform
{

template <typename Invoc1, typename Invoc2>
struct nest_fn
{
    [[no_unique_address]] Invoc1 in1;
    [[no_unique_address]] Invoc2 in2;

    template <typename Val>
    constexpr decltype(auto) operator()(Val && val) const
      noexcept(noexcept(in1(std::forward<Val>(val))) && noexcept(in2(in1(std::forward<Val>(val)))))
    {
        return in2(in1(std::forward<Val>(val)));
    }
};

//!\brief The argument passed to the invocable: the element (deref) or a copy of the iterator (!deref).
template <class UIt, bool deref>
using arg_t = std::conditional_t<deref, std::iter_reference_t<UIt>, UIt>;

template <class UIt, class Fn, bool deref = true>
concept fn_constraints = std::is_object_v<Fn> && std::regular_invocable<Fn const &, arg_t<UIt, deref>> &&
                         can_reference<std::invoke_result_t<Fn const &, arg_t<UIt, deref>>>;

} // namespace radr::detail::transform

namespace radr::detail
{

template <std::forward_iterator UIt, std::sentinel_for<UIt> Sent, typename Fn, bool deref = true>
    requires detail::transform::fn_constraints<UIt, Fn, deref>
class transform_sentinel;

/*!\brief Iterator for radr::transform.
 * \tparam UIt   Type of the underlying iterator.
 * \tparam Fn    Type of the transformer.
 * \tparam deref Whether to pass underlying value oder underlying iterator to the transformer.
 * \details
 *
 * If deref == false, the invocable receives a copy of the underlying iterator, instead of a reference
 * to the value behind it. This mechanism is used for the (internal) radr::detail::transform_it adaptor.
 */
template <std::forward_iterator UIt, typename Fn, bool deref = true>
    requires detail::transform::fn_constraints<UIt, Fn, deref>
class transform_iterator
{
    [[no_unique_address]] semiregular_box<Fn> fn{};
    [[no_unique_address]] UIt                 uit{};

    template <std::forward_iterator UIt2, typename Fn2, bool deref2>
        requires detail::transform::fn_constraints<UIt2, Fn2, deref2>
    friend class transform_iterator;
    template <std::forward_iterator UIt2, std::sentinel_for<UIt2> Sent_, typename Fn2, bool deref2>
        requires detail::transform::fn_constraints<UIt2, Fn2, deref2>
    friend class transform_sentinel;

    using arg_t = transform::arg_t<UIt, deref>;

    static constexpr bool nothrow_invoke = noexcept(std::invoke(std::declval<Fn const &>(), std::declval<arg_t>()));

    template <typename Container>
    constexpr friend auto tag_invoke(custom::rebind_iterator_tag,
                                     transform_iterator it,
                                     Container &        container_old,
                                     Container &        container_new)
    {
        it.uit = tag_invoke(custom::rebind_iterator_tag{}, it.uit, container_old, container_new);
        return it;
    }

public:
    using iterator_concept =
      std::conditional_t<std::contiguous_iterator<UIt>, std::random_access_iterator_tag, iterator_tag_t<UIt>>;
    using difference_type = std::iter_difference_t<UIt>;
    using value_type      = std::remove_cvref_t<std::invoke_result_t<Fn const &, arg_t>>;

    transform_iterator() = default;

    constexpr transform_iterator(Fn func, UIt current) : fn(std::in_place, std::move(func)), uit(std::move(current)) {}

    template <detail::different_from<UIt> OtherUIt>
    constexpr transform_iterator(transform_iterator<OtherUIt, Fn, deref> i)
        requires std::convertible_to<OtherUIt, UIt>
      : fn(std::move(i.fn)), uit(std::move(i.uit))
    {}

    constexpr UIt const & base() const & noexcept { return uit; }
    constexpr UIt         base() && { return std::move(uit); }

    constexpr Fn const & func() const & noexcept { return *fn; }
    constexpr Fn         func() && { return std::move(*fn); }

    constexpr decltype(auto) operator*() const
      noexcept(nothrow_invoke && (deref ? noexcept(*uit) : std::is_nothrow_copy_constructible_v<UIt>))
    {
        if constexpr (deref)
            return std::invoke(*fn, *uit);
        else
            return std::invoke(*fn, UIt{uit});
    }

    constexpr transform_iterator & operator++()
    {
        ++uit;
        return *this;
    }

    constexpr transform_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr transform_iterator & operator--()
        requires std::bidirectional_iterator<UIt>
    {
        --uit;
        return *this;
    }

    constexpr transform_iterator operator--(int)
        requires std::bidirectional_iterator<UIt>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr transform_iterator & operator+=(difference_type n)
        requires std::random_access_iterator<UIt>
    {
        uit += n;
        return *this;
    }

    constexpr transform_iterator & operator-=(difference_type n)
        requires std::random_access_iterator<UIt>
    {
        uit -= n;
        return *this;
    }

    constexpr decltype(auto) operator[](difference_type n) const
      noexcept(nothrow_invoke && (deref ? noexcept(uit[n]) : noexcept(uit + n)))
        requires std::random_access_iterator<UIt>
    {
        if constexpr (deref)
            return std::invoke(*fn, uit[n]);
        else
            return std::invoke(*fn, uit + n);
    }

    friend constexpr bool operator==(transform_iterator const & x, transform_iterator const & y)
    {
        return x.uit == y.uit;
    }

    friend constexpr bool operator<(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<UIt>
    {
        return x.uit < y.uit;
    }

    friend constexpr bool operator>(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<UIt>
    {
        return x.uit > y.uit;
    }

    friend constexpr bool operator<=(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<UIt>
    {
        return x.uit <= y.uit;
    }

    friend constexpr bool operator>=(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<UIt>
    {
        return x.uit >= y.uit;
    }

    friend constexpr auto operator<=>(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<UIt> && std::three_way_comparable<UIt>
    {
        return x.uit <=> y.uit;
    }

    friend constexpr transform_iterator operator+(transform_iterator i, difference_type n)
        requires std::random_access_iterator<UIt>
    {
        return transform_iterator{*i.fn, i.uit + n};
    }

    friend constexpr transform_iterator operator+(difference_type n, transform_iterator i)
        requires std::random_access_iterator<UIt>
    {
        return transform_iterator{*i.fn, i.uit + n};
    }

    friend constexpr transform_iterator operator-(transform_iterator i, difference_type n)
        requires std::random_access_iterator<UIt>
    {
        return transform_iterator{*i.fn, i.uit - n};
    }

    friend constexpr difference_type operator-(transform_iterator const & x, transform_iterator const & y)
        requires std::sized_sentinel_for<UIt, UIt>
    {
        return x.uit - y.uit;
    }

    /* std::default_sentinel_t is preserved as sentinel (instead of being wrapped in transform_sentinel) */
    friend constexpr bool operator==(transform_iterator const & x, std::default_sentinel_t)
        requires std::sentinel_for<std::default_sentinel_t, UIt>
    {
        return x.uit == std::default_sentinel;
    }

    friend constexpr difference_type operator-(transform_iterator const & x, std::default_sentinel_t)
        requires std::sized_sentinel_for<std::default_sentinel_t, UIt>
    {
        return x.uit - std::default_sentinel;
    }

    friend constexpr difference_type operator-(std::default_sentinel_t, transform_iterator const & y)
        requires std::sized_sentinel_for<std::default_sentinel_t, UIt>
    {
        return std::default_sentinel - y.uit;
    }

    friend constexpr decltype(auto) iter_move(transform_iterator const & i) noexcept(noexcept(*i))
    {
        if constexpr (std::is_lvalue_reference_v<decltype(*i)>)
            return std::move(*i);
        else
            return *i;
    }
};

template <std::forward_iterator UIt, std::sentinel_for<UIt> USen, typename Fn, bool deref>
    requires detail::transform::fn_constraints<UIt, Fn, deref>
class transform_sentinel
{
    [[no_unique_address]] USen end_{};

    template <std::forward_iterator UIt2, std::sentinel_for<UIt2> Sent_, typename Fn2, bool deref2>
        requires detail::transform::fn_constraints<UIt2, Fn2, deref2>
    friend class transform_sentinel;

    using iterator_t = transform_iterator<UIt, Fn, deref>;

public:
    transform_sentinel() = default;

    constexpr explicit transform_sentinel(USen end) : end_(end) {}

    constexpr transform_sentinel(Fn, USen end) : end_(end) {}

    template <std::forward_iterator OtherUIt, typename OtherSent>
    constexpr transform_sentinel(transform_sentinel<OtherUIt, OtherSent, Fn, deref> i)
        requires std::convertible_to<OtherSent, USen> && std::convertible_to<OtherUIt, UIt>
      : end_(std::move(i.end_))
    {}

    constexpr USen base() const { return end_; }

    /* All three go through transform_iterator::base() instead of `.current_`: GCC<=12 does not grant a
     * befriended class's friendship to functions *defined inside* that class, which is what these are. */
    friend constexpr bool operator==(iterator_t const & x, transform_sentinel const & y) { return x.base() == y.end_; }

    friend constexpr std::iter_difference_t<iterator_t> operator-(iterator_t const & x, transform_sentinel const & y)
        requires std::sized_sentinel_for<USen, UIt>
    {
        return x.base() - y.end_;
    }

    friend constexpr std::iter_difference_t<iterator_t> operator-(transform_sentinel const & x, iterator_t const & y)
        requires std::sized_sentinel_for<USen, UIt>
    {
        return x.end_ - y.base();
    }
};

template <bool deref = true>
inline constexpr auto transform_borrow = []<typename URange, typename Fn>(URange && urange, Fn fn)
{
    static_assert(borrowed_mp_range<URange>, "The constraints for radr::transform's underlying range are not met.");
    static_assert(detail::transform::fn_constraints<radr::iterator_t<URange>, Fn, deref> &&
                    detail::transform::fn_constraints<radr::const_iterator_t<URange>, Fn, deref>,
                  "The constraints for radr::transform's functor are not met.");

    // clang-format off
    constexpr auto foldable = overloaded{
    /* default is not foldable */
    [](auto, auto)
    {
        return std::pair<bool, bool>{false, false}; // second bool is never read
    },
    /* foldable if `it` is transform_iterator and sen is also transform_iterator or stateless sentinel.
     * A stateful sentinel of different type (e.g. radr::take_while's) shall NOT be folded!
     */
    []<typename UIt, typename UFn, bool UDeref, typename USen>(
        std::type_identity<transform_iterator<UIt, UFn, UDeref>>,
        std::type_identity<USen>)
    {
        return std::pair<bool, bool>{
            one_of<USen, transform_iterator<UIt, UFn, UDeref>, std::unreachable_sentinel_t, std::default_sentinel_t>,
            UDeref};
    },
    /* foldable if `it` is transform_iterator and sen is transform_sentinel */
    []<typename UIt, typename USen, typename UFn, bool UDeref>(
        std::type_identity<transform_iterator<UIt, UFn, UDeref>>,
        std::type_identity<transform_sentinel<UIt, USen, UFn, UDeref>>)
    {
        return std::pair<bool, bool>{true, UDeref};
    }};
    // clang-format on

    /* only a dereferencing transform is folded into the underlying one */
    static constexpr bool folds =
      deref && foldable(std::type_identity<iterator_t<URange>>{}, std::type_identity<sentinel_t<URange>>{}).first &&
      foldable(std::type_identity<const_iterator_t<URange>>{}, std::type_identity<const_sentinel_t<URange>>{}).first;
    static constexpr bool uderef =
      foldable(std::type_identity<iterator_t<URange>>{}, std::type_identity<sentinel_t<URange>>{}).second;

    /* creates the iterator-sentinel pair; stateless sentinels are preserved */
    constexpr auto make =
      []<typename UIt, typename USen, typename Fn2, bool D>(UIt uit, USen usen, Fn2 fn_, std::bool_constant<D>)
    {
        using it_t = transform_iterator<UIt, Fn2, D>;

        if constexpr (std::same_as<UIt, USen>)
            return std::pair{
              it_t{fn_,  std::move(uit)},
              it_t{fn_, std::move(usen)}
            };
        else if constexpr (one_of<USen, std::unreachable_sentinel_t, std::default_sentinel_t>)
            return std::pair{
              it_t{fn_, std::move(uit)},
              usen
            };
        else
            return std::pair{
              it_t{fn_, std::move(uit)},
              transform_sentinel<UIt, USen, Fn2, D>{std::move(usen)}
            };
    };

    auto adapt = [&](auto uit, auto usen)
    {
        if constexpr (folds)
        {
            auto usen_base = [&]
            {
                if constexpr (one_of<decltype(usen), std::unreachable_sentinel_t, std::default_sentinel_t>)
                    return usen;
                else
                    return std::move(usen).base();
            }();

            return make(uit.base(),
                        std::move(usen_base),
                        transform::nest_fn{uit.func(), fn},
                        std::bool_constant<uderef>{});
        }
        else
        {
            return make(std::move(uit), std::move(usen), fn, std::bool_constant<deref>{});
        }
    };

    auto [beg, end]   = adapt(radr::begin(urange), radr::end(urange));
    auto [cbeg, cend] = adapt(radr::cbegin(urange), radr::cend(urange));

    return borrowing_rad{beg, end, cbeg, cend, detail::size_or_not(urange)};
};

inline constexpr auto transform_coro = []<std::ranges::input_range URange, typename Fn>(URange && urange, Fn fn)
{
    static_assert(std::move_constructible<Fn> && std::invocable<Fn &, std::ranges::range_reference_t<URange>>,
                  "The constraints for radr::transform's functor are not met.");

    using ref_t = std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>;
    static_assert(can_reference<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>,
                  "The constraints for radr::transform's functor are not met.");

    static_assert(!container_lvalue<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    // we need to create inner functor so that it can take by value
    return [](auto urange_, Fn fn) -> radr::generator<ref_t, std::remove_cvref_t<ref_t>>
    {
        for (auto && elem : urange_)
            co_yield fn(std::forward<decltype(elem)>(elem));
    }(std::move(urange), std::move(fn));
};

//!\brief Like radr::transform, but invokes fn with (a copy of) the underlying iterator instead of the element. Multi-pass only.
inline constexpr auto transform_it = pipe_with_args_fn<void, decltype(transform_borrow<false>)>{};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Transforms a range by applying an invocable ("function") on each element.
 * \param[in] urange The underlying range.
 * \param[in] fn The invocable to apply
 * \tparam URange Type of \p urange.
 * \tparam Fn Type of \p fn.
 * \details
 *
 * ## Multi-pass adaptor
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * `std::copy_constructible<Fn>` and `std::is_object_v<Fn>`
 *   * `std::regular_invocable<Fn const &, std::ranges::range_reference_t<URange>>`
 *   * `std::regular_invocable<Fn const &, std::ranges::range_const_reference_t<URange>>`
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::sized_range
 *   * std::ranges::borrowed_range
 *   * radr::common_range
 *   * radr::constant_range
 *   * radr::infinite_mp_range
 *   * radr::mutable_range (see below)
 *
 * Since transformers usually do not return references, mutability is lost anyway, and prefixing radr::as_const can
 * result in simpler types.
 *
 * ### Notable differences to std::views::transform
 *
 * * radr::transform on borrowed ranges is always borrowed; std::views::transform never is.
 * * \p fn is stored within the iterator, not the range.
 * * Multiple chained transforms are folded into one.
 *
 * ### Performance
 *
 * * For a single, stateless \p fn, this adaptor produces the same instructions as std::views::transform.
 * * If \p fn has state, in-iterator storage can yield measurable speed-ups over std::views::transform
 *   (especially with Clang).
 * * The effect is stronger if multiple adaptors are chained.
 *
 * On the other hand, stateful \p fn leads to a larger iterator then in `std::`.
 *
 * ## Single-pass adaptor
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *   * `std::move_constructible<Fn>` and `std::is_object_v<Fn>`
 *   * `std::invocable<Fn &, std::ranges::range_reference_t<URange>>`
 *
 * The single-pass adaptor allows (observable) changes in the transformer (std::invocable instead of
 * std::regular_invocable).
 *
 */
inline constexpr auto transform = detail::pipe_with_args_fn{detail::transform_coro, detail::transform_borrow<>};
} // namespace cpo
} // namespace radr
