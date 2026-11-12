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
#include "../detail/generic_iterator.hpp"
#include "../detail/pipe.hpp"
#include "../detail/semiregular_box.hpp"
#include "../generator.hpp"
#include "radr/range_access.hpp"

namespace radr::detail::transform
{

//!\brief Invokes \p in2 on the result of \p in1.
template <typename Invoc1, typename Invoc2>
struct nest_fn
{
    [[no_unique_address]] Invoc1 in1;
    [[no_unique_address]] Invoc2 in2;

    template <typename... Vals>
    constexpr decltype(auto) operator()(Vals &&... vals) const
      noexcept(noexcept(in1(std::forward<Vals>(vals)...)) && noexcept(in2(in1(std::forward<Vals>(vals)...))))
    {
        return in2(in1(std::forward<Vals>(vals)...));
    }
};

//!\brief The argument passed to the invocable: the element (deref) or a copy of the iterator (!deref).
template <class UIt, bool deref>
using arg_t = std::conditional_t<deref, std::iter_reference_t<UIt>, UIt>;

//!\brief The value type of radr::detail::indirect_transform_iterator; Fn may provide it as `Fn::value_type<UIt>`.
template <class UIt, class Fn>
struct it_value_type
{
    using type = std::remove_cvref_t<std::invoke_result_t<Fn const &, UIt>>;
};

template <class UIt, class Fn>
    requires requires { typename Fn::template value_type<UIt>; }
struct it_value_type<UIt, Fn>
{
    using type = typename Fn::template value_type<UIt>;
};

template <class UIt, class Fn, bool deref = true>
concept fn_constraints = std::is_object_v<Fn> && std::regular_invocable<Fn const &, arg_t<UIt, deref>> &&
                         can_reference<std::invoke_result_t<Fn const &, arg_t<UIt, deref>>>;

} // namespace radr::detail::transform

namespace radr::detail
{

//!\brief Iterator of radr::transform: invokes \p Fn on the element.
template <std::forward_iterator UIt, typename Fn>
    requires transform::fn_constraints<UIt, Fn>
class transform_iterator : public generic_iterator_base<transform_iterator<UIt, Fn>, UIt>
{
    using base_t = generic_iterator_base<transform_iterator, UIt>;

    [[no_unique_address]] semiregular_box<Fn> fn{};

    template <std::forward_iterator UIt2, typename Fn2>
        requires transform::fn_constraints<UIt2, Fn2>
    friend class transform_iterator;

public:
    using value_type = std::remove_cvref_t<std::invoke_result_t<Fn const &, std::iter_reference_t<UIt>>>;
    using typename base_t::difference_type;

    transform_iterator() = default;

    constexpr transform_iterator(Fn f, UIt u) : base_t{std::move(u)}, fn{std::in_place, std::move(f)} {}

    template <different_from<UIt> UIt2>
        requires std::convertible_to<UIt2, UIt>
    constexpr transform_iterator(transform_iterator<UIt2, Fn> i) : base_t{std::move(i).base()}, fn{std::move(i.fn)}
    {}

    constexpr Fn const & func() const & noexcept { return *fn; }
    constexpr Fn         func() && { return std::move(*fn); }

    constexpr decltype(auto) operator*() const noexcept(noexcept(std::invoke(*fn, *this->base())))
    {
        return std::invoke(*fn, *this->base());
    }

    //!\brief Preserves the codegen of fn(uit[n]).
    constexpr decltype(auto) operator[](difference_type n) const noexcept(noexcept(std::invoke(*fn, this->base()[n])))
        requires std::random_access_iterator<UIt>
    {
        return std::invoke(*fn, this->base()[n]);
    }

    //!\brief This iterator with \p fn2 applied after Fn; used by radr::transform's folding.
    template <typename Fn2>
    constexpr auto fold(Fn2 fn2) const
    {
        using fn_t = transform::nest_fn<Fn, Fn2>;
        return transform_iterator<UIt, fn_t>{
          fn_t{*fn, std::move(fn2)},
          this->base()
        };
    }
};

/*!\brief Iterator of radr::detail::transform_it: invokes \p Fn on a copy of the underlying iterator.
 * \details Fn may provide the value type as `Fn::value_type<UIt>`.
 */
template <std::forward_iterator UIt, typename Fn>
    requires transform::fn_constraints<UIt, Fn, false>
class indirect_transform_iterator : public generic_iterator_base<indirect_transform_iterator<UIt, Fn>, UIt>
{
    using base_t = generic_iterator_base<indirect_transform_iterator, UIt>;

    [[no_unique_address]] semiregular_box<Fn> fn{};

    template <std::forward_iterator UIt2, typename Fn2>
        requires transform::fn_constraints<UIt2, Fn2, false>
    friend class indirect_transform_iterator;

public:
    using value_type = typename transform::it_value_type<UIt, Fn>::type;
    using typename base_t::difference_type;

    indirect_transform_iterator() = default;

    constexpr indirect_transform_iterator(Fn f, UIt u) : base_t{std::move(u)}, fn{std::in_place, std::move(f)} {}

    template <different_from<UIt> UIt2>
        requires std::convertible_to<UIt2, UIt>
    constexpr indirect_transform_iterator(indirect_transform_iterator<UIt2, Fn> i) :
      base_t{std::move(i).base()}, fn{std::move(i.fn)}
    {}

    constexpr Fn const & func() const & noexcept { return *fn; }
    constexpr Fn         func() && { return std::move(*fn); }

    constexpr decltype(auto) operator*() const
      noexcept(noexcept(std::invoke(*fn, UIt{this->base()})) && std::is_nothrow_copy_constructible_v<UIt>)
    {
        return std::invoke(*fn, UIt{this->base()});
    }

    //!\brief Avoids copying fn.
    constexpr decltype(auto) operator[](difference_type n) const noexcept(noexcept(std::invoke(*fn, this->base() + n)))
        requires std::random_access_iterator<UIt>
    {
        return std::invoke(*fn, this->base() + n);
    }

    //!\brief This iterator with \p fn2 applied after Fn; used by radr::transform's folding.
    template <typename Fn2>
    constexpr auto fold(Fn2 fn2) const
    {
        using fn_t = transform::nest_fn<Fn, Fn2>;
        return indirect_transform_iterator<UIt, fn_t>{
          fn_t{*fn, std::move(fn2)},
          this->base()
        };
    }
};

inline constexpr auto transform_borrow = []<typename URange, typename Fn>(URange && urange, Fn fn)
{
    static_assert(borrowed_mp_range<URange>, "The constraints for radr::transform's underlying range are not met.");
    static_assert(detail::transform::fn_constraints<radr::iterator_t<URange>, Fn> &&
                    detail::transform::fn_constraints<radr::const_iterator_t<URange>, Fn>,
                  "The constraints for radr::transform's functor are not met.");

    using UIt   = iterator_t<URange>;
    using USen  = sentinel_t<URange>;
    using UCIt  = const_iterator_t<URange>;
    using UCSen = const_sentinel_t<URange>;

    static constexpr bool folds =
      (requires(UIt it) { it.fold(fn); }) && (requires(UCIt it) { it.fold(fn); }) &&
      (one_of<USen, UIt, std::unreachable_sentinel_t, std::default_sentinel_t> || is_generic_sentinel<USen>)&&(
        one_of<UCSen, UCIt, std::unreachable_sentinel_t, std::default_sentinel_t> || is_generic_sentinel<UCSen>);

    auto adapt = [&]<typename UIt, typename USen>(UIt uit, USen usen)
    {
        if constexpr (folds)
        {
            if constexpr (std::same_as<UIt, USen>)
                return std::pair{uit.fold(fn), usen.fold(fn)};
            else
                return std::pair{uit.fold(fn), std::move(usen)};
        }
        else
        {
            return generic_wrap(std::move(uit),
                                std::move(usen),
                                [&]<typename UIt2>(UIt2 uit2) {
                return transform_iterator<UIt2, Fn>{fn, std::move(uit2)};
            });
        }
    };

    auto [beg, end]   = adapt(radr::begin(urange), radr::end(urange));
    auto [cbeg, cend] = adapt(radr::cbegin(urange), radr::cend(urange));

    return borrowing_rad{beg, end, cbeg, cend, detail::size_or_not(urange)};
};

//!\brief Never folds into the underlying range.
inline constexpr auto transform_it_borrow = []<typename URange, typename Fn>(URange && urange, Fn fn)
{
    static_assert(borrowed_mp_range<URange>, "The constraints for radr::transform's underlying range are not met.");
    static_assert(detail::transform::fn_constraints<radr::iterator_t<URange>, Fn, false> &&
                    detail::transform::fn_constraints<radr::const_iterator_t<URange>, Fn, false>,
                  "The constraints for radr::transform's functor are not met.");

    return generic_borrow(std::forward<URange>(urange),
                          [&]<typename UIt>(UIt uit) {
        return indirect_transform_iterator<UIt, Fn>{fn, std::move(uit)};
    });
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
inline constexpr auto transform_it = pipe_with_args_fn<void, decltype(transform_it_borrow)>{};

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
 * * Multiple chained transforms are folded into one; so is a transform over radr::adjacent or
 *   radr::adjacent_transform on a radr::safely_indexable_range.
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
inline constexpr auto transform = detail::pipe_with_args_fn{detail::transform_coro, detail::transform_borrow};
} // namespace cpo
} // namespace radr
