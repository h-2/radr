// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023-2025 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <functional>
#include <ranges>

#include "../detail/detail.hpp"
#include "transform.hpp"

namespace radr::detail
{

template <size_t I>
struct elements_fn
{
    template <typename Tuple>
    constexpr decltype(auto) operator()(Tuple && tuple) const
    {
        static_assert(tuple_like<Tuple>, "The element type of the underlying range must be a tuple or pair.");
        static_assert(std::tuple_size_v<std::remove_reference_t<Tuple>> > I,
                      "The element number requested by radr::elements was larger than the tuple-size.");

        if constexpr (std::is_reference_v<Tuple>)
        {
            return std::get<I>(tuple);
        }
        else
        {
            using E = std::remove_cv_t<std::tuple_element_t<I, Tuple>>;
            return static_cast<E>(std::get<I>(tuple));
        }
    }
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Projects a range of tuples to the I-th element of the tuple.
 * \param urange The underlying range.
 * \tparam URange Type of \p.
 * \tparam I The index of the element to be projected to.
 *
 * ### Multi-pass adaptor
 *
 * Requirements:
 *  * `radr::mp_range<URange>`
 *  * `radr::detail::tuple_like<std::ranges::range_reference_t<URange>>`
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::common_range
 *   * radr::constant_range
 *   * radr::mutable_range
 *
 * ### Single-pass adaptor
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 */

template <size_t I>
inline constexpr auto elements = radr::transform(detail::elements_fn<I>{});

/*!\brief Projects a range of tuples to the 0th element of the tuple.
 * \param urange The underlying range.
 * \tparam URange Type of \p.
 *
 * See radr::elements.
 */
inline constexpr auto keys = elements<0>;

/*!\brief Projects a range of tuples to the 1st element of the tuple.
 * \param urange The underlying range.
 * \tparam URange Type of \p.
 *
 * See radr::elements.
 */
inline constexpr auto values = elements<1>;
} // namespace cpo
} // namespace radr
