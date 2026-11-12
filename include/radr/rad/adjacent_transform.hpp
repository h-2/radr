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

#include <cstddef>
#include <utility>

#include "radr/concepts.hpp"
#include "radr/detail/adjacent_iterator.hpp"
#include "radr/detail/generic_iterator.hpp"
#include "radr/detail/pipe.hpp"
#include "radr/rad/transform.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{

//!\brief Iterator of radr::adjacent_transform on radr::safely_indexable_range; stores one iterator only.
template <std::random_access_iterator UIt, size_t N, typename Fn>
class adjacent_transform_ra_iterator : public generic_iterator_base<adjacent_transform_ra_iterator<UIt, N, Fn>, UIt>
{
    using base_t = generic_iterator_base<adjacent_transform_ra_iterator, UIt>;

    [[no_unique_address]] semiregular_box<Fn> fn{};

    template <std::random_access_iterator UIt2, size_t N2, typename Fn2>
    friend class adjacent_transform_ra_iterator;

    constexpr decltype(auto) read(UIt const & uit) const
    {
        return with_adj_window<N>(uit,
                                  [&](auto const &... its) -> decltype(auto) { return std::invoke(*fn, *its...); });
    }

public:
    using value_type = typename adjacent_window_value<zip_policy_transform<Fn>, UIt, std::make_index_sequence<N>>::type;
    using typename base_t::difference_type;

    adjacent_transform_ra_iterator() = default;

    constexpr adjacent_transform_ra_iterator(Fn f, UIt u) : base_t{std::move(u)}, fn{std::in_place, std::move(f)} {}

    template <different_from<UIt> UIt2>
        requires std::convertible_to<UIt2, UIt>
    constexpr adjacent_transform_ra_iterator(adjacent_transform_ra_iterator<UIt2, N, Fn> i) :
      base_t{std::move(i).base()}, fn{std::move(i.fn)}
    {}

    constexpr Fn const & func() const & noexcept { return *fn; }
    constexpr Fn         func() && { return std::move(*fn); }

    constexpr decltype(auto) operator*() const { return read(this->base()); }

    //!\brief Avoids copying fn.
    constexpr decltype(auto) operator[](difference_type n) const { return read(this->base() + n); }

    //!\brief This iterator with \p fn2 applied after Fn; used by radr::transform's folding.
    template <typename Fn2>
    constexpr auto fold(Fn2 fn2) const
    {
        using fn_t = transform::nest_fn<Fn, Fn2>;
        return adjacent_transform_ra_iterator<UIt, N, fn_t>{
          fn_t{*fn, std::move(fn2)},
          this->base()
        };
    }
};

template <ptrdiff_t N>
inline constexpr auto adjacent_transform_borrow = []<typename URange, typename Fn>(URange && urange, Fn fn)
{
    static_assert(N > 0, "You must select N > 0 for radr::adjacent_transform.");

    static_assert(borrowed_mp_range<URange>,
                  "The constraints for radr::adjacent_transform's underlying range are not met.");

    using seq_t = std::make_index_sequence<N>;
    static_assert(zip_policy_transform_constraints_n<Fn, iterator_t<URange>, seq_t> &&
                    zip_policy_transform_constraints_n<Fn, const_iterator_t<URange>, seq_t>,
                  "The constraints for radr::adjacent_transform's functor are not met; note that it is invoked with N "
                  "arguments.");

    if constexpr (safely_indexable_range<URange>)
    {
        return adjacent_borrow_ra<N>(std::forward<URange>(urange),
                                     [&]<typename UIt>(UIt uit) {
            return adjacent_transform_ra_iterator<UIt, N, Fn>{fn, std::move(uit)};
        });
    }
    else
    {
        return adjacent_borrow_impl_regular<N>(
          zip_policy_transform<Fn>{
            semiregular_box<Fn>{std::in_place, std::move(fn)}
        },
          std::forward<URange>(urange));
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Slides a window of fixed size N over the range and applies an invocable to every window.
 * \tparam N The size of the window (>= 1).
 * \tparam URange Type of \p urange.
 * \tparam Fn Type of \p fn.
 * \param[in] urange The underlying range.
 * \param[in] fn The invocable to apply; it is called with N arguments, not with a tuple.
 * \details
 *
 * This adaptor is similar to `radr::adjacent<N> | radr::transform(fn)`, but \p fn is invoked as `fn(a, b, …)` and no
 * intermediate std::tuple is created.
 *
 * In contrast to the regular zip-family of adaptors and factories, the `*_transform`-variants do not require C++23.
 *
 * ### Multi-pass adaptor
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * Requirements on \p fn : std::copy_constructible, std::is_object_v, std::regular_invocable (`fn const &` with
 *     N copies of \p urange 's `reference_t` and with N copies of its `const_reference_t` )
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::common_range
 *   * radr::constant_range
 *   * radr::mutable_range (see below)
 *
 * Since transformers usually do not return references, mutability is lost anyway, and prefixing radr::as_const can
 * result in simpler types.
 *
 * ### Notable difference to std::views::adjacent_transform
 *
 *  * This adaptor does not require C++23.
 *  * \p N == 0 is not supported.
 *
 * ### Single-pass adaptor
 *
 * radr::adjacent_transform cannot be created on single-pass ranges.
 */
template <ptrdiff_t N>
inline constexpr auto adjacent_transform =
  detail::pipe_with_args_fn<void, decltype(detail::adjacent_transform_borrow<N>)>{};
} // namespace cpo
} // namespace radr
