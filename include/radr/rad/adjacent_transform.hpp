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
#include "radr/detail/pipe.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{

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

    return adjacent_borrow_impl<N>(
      zip_policy_transform<Fn>{
        semiregular_box<Fn>{std::in_place, std::move(fn)}
    },
      std::forward<URange>(urange));
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
