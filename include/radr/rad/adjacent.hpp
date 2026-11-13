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

#include "radr/detail/adjacent_iterator.hpp"
#include "radr/rad/adjacent_transform.hpp"
#include "radr/version.hpp"

namespace radr::detail
{

//!\brief Builds the std::tuple of references of radr::adjacent from the elements.
struct adjacent_tuple_fn
{
    template <typename... Refs>
    constexpr auto operator()(Refs &&... refs) const
    {
        return std::tuple<Refs...>(std::forward<Refs>(refs)...);
    }
};

//!\brief Iterator of radr::adjacent on radr::safely_indexable_range; stores one iterator only.
template <std::random_access_iterator UIt, size_t N>
class adjacent_ra_iterator : public generic_iterator_base<adjacent_ra_iterator<UIt, N>, UIt>
{
    using base_t = generic_iterator_base<adjacent_ra_iterator, UIt>;

    template <std::random_access_iterator UIt2, size_t N2>
    friend class adjacent_ra_iterator;

public:
    using value_type = typename adjacent_window_value<zip_policy_tuple, UIt, std::make_index_sequence<N>>::type;
    using typename base_t::difference_type;

    adjacent_ra_iterator() = default;

    constexpr explicit adjacent_ra_iterator(UIt u) : base_t{std::move(u)} {}

    template <different_from<UIt> UIt2>
        requires std::convertible_to<UIt2, UIt>
    constexpr adjacent_ra_iterator(adjacent_ra_iterator<UIt2, N> i) : base_t{std::move(i).base()}
    {}

    constexpr auto operator*() const { return with_adj_window<N>(this->base(), zip_policy_tuple{}); }

    friend constexpr auto iter_move(adjacent_ra_iterator const & i)
    {
        return with_adj_window<N>(i.base(),
                                  [](auto const &... its)
        {
            return std::tuple<std::iter_rvalue_reference_t<std::remove_cvref_t<decltype(its)>>...>(
              std::ranges::iter_move(its)...);
        });
    }

    friend constexpr void iter_swap(adjacent_ra_iterator const & x, adjacent_ra_iterator const & y)
        requires std::indirectly_swappable<UIt>
    {
        [&]<size_t... Is>(std::array<UIt, N> const & l, std::array<UIt, N> const & r, std::index_sequence<Is...>)
        {
            (std::ranges::iter_swap(l[Is], r[Is]), ...);
        }(make_adj_window_its<N>(x.base()), make_adj_window_its<N>(y.base()), std::make_index_sequence<N>{});
    }

    //!\brief radr::adjacent_transform_ra_iterator with \p fn2 applied to the tuple; used by radr::transform's folding.
    template <typename Fn2>
    constexpr auto fold(Fn2 fn2) const
    {
        using fn_t = transform::nest_fn<adjacent_tuple_fn, Fn2>;
        return adjacent_transform_ra_iterator<UIt, N, fn_t>{
          fn_t{{}, std::move(fn2)},
          this->base()
        };
    }
};

template <ptrdiff_t N>
inline constexpr auto adjacent_borrow = []<std::ranges::borrowed_range URange>(URange && urange)
    requires std::ranges::forward_range<URange>
{
    static_assert(N > 0, "You must select N > 0 for radr::adjacent.");

    if constexpr (safely_indexable_range<URange>)
        return adjacent_borrow_ra<N>(std::forward<URange>(urange),
                                     []<typename UIt>(UIt uit)
        { return adjacent_ra_iterator<UIt, N>{std::move(uit)}; });
    else
        return adjacent_borrow_impl_regular<N>(zip_policy_tuple{}, std::forward<URange>(urange));
};

} // namespace radr::detail

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include <algorithm>
#    include <array>
#    include <ranges>

#    include "radr/class/zip_container.hpp"
#    include "radr/concepts.hpp"
#    include "radr/detail/pipe.hpp"

namespace radr
{

inline namespace cpo
{
/*!\brief radr::adjacent<K>(urange)
 * \tparam K The size of the tuple (>= 1).
 * \param urange The underlying range.
 *
 * Turns a range into a range of overlapping tuples, each of size N.
 *
 * Requires C++23.
 *
 * Be aware of the following similar adaptors:
 *
 * |                     |  parameter        | element-type  |
 * |---------------------|-------------------|---------------|
 * | radr::adjacent      | compile-time      | tuple         |
 * | radr::slide_counted | compile-time      | range         |
 * | radr::slide         | runtime           | range         |
 *
 * ## Multi-pass adaptor
 *
 * Requirements on \p urange :
 *   * radr::mp_range
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::common_range
 *   * radr::constant_range
 *   * radr::mutable_range
 *   * radr::infinite_mp_range
 *
 * ### Notable differences to std::views::adjacent
 *
 * For underlying ranges that model radr::safely_indexable_range, this adaptor's iterator stores only one underlying
 * iterator.
 *
 * ### Performance
 *
 * Can be faster than std::views::adjacent, radr::slide_counted and radr::slide on bidirectional ranges.
 *
 * Can be faster than std::views::adjacent on random access ranges, especially ones that are
 * not contiguous.
 *
 * ## Single-pass adaptor
 *
 * radr::adjacent cannot be created on single-pass ranges.
 */
template <ptrdiff_t K>
inline constexpr auto adjacent = detail::pipe_without_args_fn<void, decltype(detail::adjacent_borrow<K>)>{};
} // namespace cpo
} // namespace radr

#endif
