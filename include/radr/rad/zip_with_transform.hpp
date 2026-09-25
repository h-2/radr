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

#include <functional>
#include <ranges>
#include <tuple>
#include <utility>

#include "radr/class/zip_container.hpp"
#include "radr/concepts.hpp"
#include "radr/custom/subborrow.hpp"
#include "radr/detail/detail.hpp"
#include "radr/detail/pipe.hpp"
#include "radr/generator.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{

inline constexpr auto zip_with_transform_borrow =
  []<typename URange, typename Fn, typename... OtherRanges>(URange && urange, Fn fn, OtherRanges &&... others)
{
    static_assert(borrowed_mp_range<URange>,
                  "The constraints for radr::zip_with_transform's underlying (primary) range are not met.");

    static_assert((safe_indirect_mp_range<OtherRanges> && ...),
                  "All ranges passed to radr::zip_with_transform after the functor need to be \n"
                  "  1) multi-pass ranges; to create a single-pass adaptor, wrap the first argument into "
                  "radr::to_single_pass.\n"
                  "  2) safe/explicit indirections; did you forget to wrap a container in std::ref() or std::cref()?\n"
                  "     To pass rvalues of containers here, use the radr::zip_transform factory instead.");

    static_assert(
      zip_policy_transform_constraints<Fn, iterator_t<borrow_t<URange>>, iterator_t<borrow_t<OtherRanges>>...> &&
        zip_policy_transform_constraints<Fn,
                                         const_iterator_t<borrow_t<URange>>,
                                         const_iterator_t<borrow_t<OtherRanges>>...>,
      "The constraints for radr::zip_with_transform's functor are not met.");

    return zip_with_borrow_impl<zip_iterator_kind::adaptor>(
      zip_policy_transform<Fn>{
        semiregular_box<Fn>{std::in_place, std::move(fn)}
    },
      radr::borrow(std::forward<URange>(urange)),
      radr::borrow(std::forward<OtherRanges>(others))...);
};

inline constexpr auto zip_with_transform_coro =
  []<typename URange, typename Fn, typename... OtherRanges>(URange && urange, Fn fn, OtherRanges &&... others)
{
    static_assert((fwdable_range<OtherRanges> && ...),
                  "All ranges passed to radr::zip_with_transform after the functor must model radr::fwdable_range.");
    static_assert(((!container_lvalue<OtherRanges>)&&...), RADR_ASSERTSTRING_RVALUE);

    auto impl = []<typename... URanges>(Fn fn_, URanges &&... uranges)
    {
        static_assert(std::is_object_v<Fn> && std::invocable<Fn &, std::ranges::range_reference_t<URanges>...>,
                      "The constraints for radr::zip_with_transform's functor are not met.");

        using ref_t = std::invoke_result_t<Fn &, std::ranges::range_reference_t<URanges>...>;
        static_assert(can_reference<ref_t>, "The constraints for radr::zip_with_transform's functor are not met.");

        // we need to create inner functor so that it can take by value
        return [](Fn fn__, auto... uranges_) -> radr::generator<ref_t, std::remove_cvref_t<ref_t>>
        {
            std::tuple<iterator_t<URanges>...>       its{radr::begin(uranges_)...};
            std::tuple<sentinel_t<URanges>...> const ends{radr::end(uranges_)...};

            auto at_end = [&]<size_t... I>(std::index_sequence<I...>)
            {
                return ((std::get<I>(its) == std::get<I>(ends)) || ...);
            };

            while (!at_end(std::make_index_sequence<sizeof...(uranges)>{}))
            {
                co_yield std::apply([&fn__](auto &... it) -> decltype(auto) { return std::invoke(fn__, *it...); }, its);
                tuple_for_each([](auto & it) { ++it; }, its);
            }
        }(std::move(fn_), std::move(uranges)...);
    };

    return impl(std::move(fn), RADR_FWD(urange), RADR_FWD(others)...);
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Zips ranges with the given one and applies an invocable to every set of corresponding elements.
 * \tparam URange Type of \p urange.
 * \tparam Fn Type of \p fn.
 * \tparam OtherRanges Types of \p other_ranges.
 * \param[in] urange The underlying range.
 * \param[in] fn The invocable to apply; it is called with N arguments, not with a tuple.
 * \param[in] other_ranges A pack of the other ranges; must be borrowed or wrapped in std::ref or std::cref.
 * \details
 *
 * This adaptor is similar to `radr::zip_with(urange, other_ranges...) | radr::transform(fn)`, but \p fn is invoked as
 * `fn(a, b, …)` and no intermediate std::tuple is created.
 *
 * In contrast to the regular zip-family of adaptors and factories, the `*_transform`-variants do not require C++23.
 *
 * ## Comparison with other adaptors
 *
 * |                                         | std::views::zip_transform                          | radr::zip_transform                                | radr::zip_with_transform                                 |
 * |-----------------------------------------|:--------------------------------------------------:|:--------------------------------------------------:|:--------------------------------------------------------:|
 * | minimum number of ranges                |                         0                          |                         1                          |                            1                             |
 * | lvalue of container for \p urange       |                        yes                         |                  std::ref-wrapped                  |                     std::ref-wrapped                     |
 * | lvalue of container for \p other_ranges |                        yes                         |                  std::ref-wrapped                  |                     std::ref-wrapped                     |
 * | rvalue of container for \p urange       |                        yes                         |                        yes                         |                           yes                            |
 * | rvalue of container for \p other_ranges |                        yes                         |                        yes                         |                            no                            |
 * | direct ("factory") call pattern         | `s::v::zip_transform(fn, urange, other_ranges...)` | `radr::zip_transform(fn, urange, other_ranges...)` | `radr::zip_with_transform(urange, fn, other_ranges...)`  |
 * | pipe ("adaptor") call pattern           |                         no                         |                         no                         | `urange | radr::zip_with_transform(fn, other_ranges...)` |
 * | closure ("stored") call pattern         |                         no                         |                         no                         | `radr::zip_with_transform(fn, other_ranges...)(urange)`  |
 *
 * Note the difference in argument order for the direct call pattern!
 *
 * In contrast to radr::zip_with, there is no issue with the direct call pattern for radr::zip_with_transform.
 *
 * ## Multi-pass adaptor
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * Each type in \p OtherRanges needs to model `radr::safe_indirect_mp_range` (wrap containers in `std::ref()`).
 *   * Requirements on \p Fn : std::copy_constructible, std::is_object_v, std::regular_invocable (`fn const &` with
 *     the `reference_t` s and with the `const_reference_t` s of all underlying ranges)
 *
 * This adaptor preserves, if all underlying ranges provide it:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::constant_range
 *   * radr::mutable_range (see below)
 *
 * It models radr::common_range if all underlying ranges model radr::common_range and at least one does **not** model
 * std::ranges::bidirectional_range.
 *
 * Additionally, if at least one range models std::ranges::sized_range and the others model radr::safely_indexable_range
 * (random-access + infinite), it also models std::ranges::sized_range and radr::common_range.
 *
 * Since transformers usually do not return references, mutability is lost anyway, and prefixing radr::as_const can
 * result in simpler types.
 *
 * ### Notable differences to std::views::zip_transform
 *
 * * At least one range needs to be given.
 * * Preservation of size+common on mixed sized/infinite; see above.
 *
 * The implementation of radr::zip_with_transform follows radr::zip_with (adaptor) and not radr::zip (factory). This implies:
 *   * You can pipe into radr::zip_with_transform (which you cannot into std::views::zip_transform); see "call patterns" above.
 *   * \p urange, the first/main underlying range can be an rvalue of a container—but the \p other_ranges cannot.
 *   * If you want to zip more than one container by rvalue, use radr::zip_transform instead.
 *
 * ## Single-pass adaptor
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *   * Each type in \p OtherRanges needs to model `radr::fwdable_range` (wrap containers in `std::ref()`).
 *   * Requirements on \p fn : std::move_constructible, std::is_object_v, std::invocable (`fn &` with the
 *     `reference_t` s of all underlying ranges)
 *
 * The single-pass adaptor allows (observable) changes in the transformer (std::invocable instead of
 * std::regular_invocable).
 */
inline constexpr auto zip_with_transform =
  detail::pipe_with_args_fn{detail::zip_with_transform_coro, detail::zip_with_transform_borrow};
} // namespace cpo
} // namespace radr
