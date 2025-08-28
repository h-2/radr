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

#include "radr/concepts.hpp"
#include "radr/custom/subborrow.hpp"
#include "radr/generator.hpp"
#include "radr/version.hpp"

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include "radr/class/zip_rng.hpp"

namespace radr::detail
{

// for zip_with
inline constexpr auto zip_with_borrow =
  []<typename URange, typename... OtherRanges>(URange && urange, OtherRanges &&... others)
{
    // static_assert(borrowed_mp_range<URange>,
    //               "The constraints for radr::zip_with's underlying (primary) range are not met.");

    static_assert((safe_indirect_mp_range<OtherRanges> && ...),
                  "All ranges passed to radr::zip_with after the first need to be \n"
                  "  1) multi-pass ranges; to create a single-pass adaptor, wrap the first argument into "
                  "radr::to_single_pass.\n"
                  "  2) safe/explicit indirections; did you forget to wrap a container in std::ref() or std::cref()?");

    constexpr auto impl = []<typename... URanges>(URanges &&... rngs)
    {
        auto beg  = zip_iterator{radr::begin(rngs)...};
        auto cbeg = zip_iterator{radr::cbegin(rngs)...};

        /* all infinite → result infinite */
        if constexpr ((infinite_mp_range<URanges> && ...))
        {
            return borrowing_rad{beg, std::unreachable_sentinel, cbeg, std::unreachable_sentinel};
        }
        /* all RA+sized or RA+infinite (but at least one non-infinite) → result RA+sized */
        else if constexpr ((safely_indexable_range<URanges> && ...))
        {
            auto const s    = min_range_weak_size(rngs...);
            auto       end  = beg + s;
            auto       cend = cbeg + s;

            return borrowing_rad{beg, end, cbeg, cend, s};
        }
        /* all common and at least one uni-directional → result common */
        else if constexpr ((common_range<URanges> && ...) &&
                           (sizeof...(URanges) == 1 || !(std::ranges::bidirectional_range<URanges> && ...)))
        {
            auto end  = zip_iterator{radr::end(rngs)...};
            auto cend = zip_iterator{radr::cend(rngs)...};

            if constexpr ((std::ranges::sized_range<URanges> && ...))
            {
                auto const s = min_range_weak_size(rngs...);
                return borrowing_rad{beg, end, cbeg, cend, s};
            }
            else
            {
                return borrowing_rad{beg, end, cbeg, cend, not_size{}};
            }
        }
        /* all other cases */
        else
        {
            auto end  = zip_sentinel{beg, std::make_tuple(radr::end(rngs)...)};
            auto cend = zip_sentinel{cbeg, std::make_tuple(radr::cend(rngs)...)};

            if constexpr ((std::ranges::sized_range<URanges> && ...))
            {
                auto const s = min_range_weak_size(rngs...);
                return borrowing_rad{beg, end, cbeg, cend, s};
            }
            else
            {
                return borrowing_rad{beg, end, cbeg, cend, not_size{}};
            }
        }
    };

    return impl(radr::borrow(std::forward<URange>(urange)), radr::borrow(std::forward<OtherRanges>(others))...);
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{

/*!\brief Zips multi-pass ranges.
 * \tparam URanges Type of \p uranges.
 * \param[in] uranges A pack of ranges.
 * \details
 *
 * |                                            |  std::views::zip |  radr::zip       |  radr::zip_with      |
 * |--------------------------------------------|:----------------:|:----------------:|:--------------------:|
 * | minimum arguments                          |   0              |   1              |                  1   |
 * | pipe into                                  |    no            |    no            |               yes    |
 * | lvalues of containers allowed (first arg)  | yes              | std::ref-wrapped |    std::ref-wrapped  |
 * | lvalues of containers allowed (other args) | yes              | std::ref-wrapped |    std::ref-wrapped  |
 * | rvalues of containers allowed (first arg)  | yes              | yes              |      yes             |
 * | rvalues of containers allowed (other args) | yes              | yes              |      no              |
 *
 * Use `radr::zip` when you need to capture more than one container by rvalue.
 *
 * Use `radr::zip_with` if you need pipe-support.
 *
 * ### Concepts
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * LValues of containers need to be wrapped in `std::ref()` or `std::cref()`.
 *
 * This adaptor preserves, if all underlying ranges provide it:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::constant_range
 *   * radr::mutable_range (see below)
 *
 * Additionally, it models std::ranges::sized_range if all underlying ranges model radr::safely_indexable_range and
 * at least one range models std::ranges::sized_range.
 *
 * It models radr::common_range, if one of the following conditions is met:
 *   * All underlying ranges model radr::common_range and it least one does **not** model std::ranges::bidirectional_range.
 *   * Or: all underlying ranges model radr::safely_indexable_range and at least one range models std::ranges::sized_range.
 *
 */

inline constexpr auto zip = []<typename... Ranges>(Ranges &&... ranges)
{
    if constexpr ((safe_indirect_mp_range<Ranges> && ...))
    {
        return detail::zip_with_borrow(std::forward<Ranges>(ranges)...);
    }
    else if constexpr (((mp_range<Ranges> || ref_wrapped_mp_range<Ranges>)&&...))
    {
        static_assert((!container_lvalue<Ranges> && ...),
                      "Do not pass lvalues of containers to radr::zip. "
                      "To store copies, pass copies; to store references, wrap inputs in std::ref().");
        return zip_rng{RADR_FWD(ranges)...};
    }
    else
    {
        static_assert(false, "To zip over single-pass ranges, use radr::zip_sp.");
    }
};

/*!\brief Zips single-pass ranges.
 * \tparam URanges Type of \p uranges.
 * \param[in] uranges A pack of ranges.
 * \details
 *
 * Zips multiple ranges (single-pass and/or multi-pass) into a combined single-pass range.
 *
 * ### Concepts
 *
 * Requirements for every argument:
 *   * Must model radr::fwdable_range.
 *   * Must not model radr::container_lvalue.
 */
inline constexpr auto zip_sp = []<typename... URanges_>(URanges_ &&... uranges_)
{
    static_assert((fwdable_range<URanges_> && ...), "Arguments to radr::zip_sp must model radr::fwdable_range.");
    static_assert(((!container_lvalue<URanges_>)&&...), RADR_ASSERTSTRING_RVALUE);

    auto impl = []<typename... URanges>(URanges &&... uranges)
    {
        static constexpr bool rval_workaround =
          ((!std::copy_constructible<std::ranges::range_reference_t<URanges>>) || ...);

        using val_t = std::tuple<std::ranges::range_value_t<URanges>...>;
        using ref_t = std::conditional_t<rval_workaround,
                                         std::tuple<std::ranges::range_reference_t<URanges>...> &&,
                                         std::tuple<std::ranges::range_reference_t<URanges>...>>;

        return [](auto... uranges_) -> radr::generator<ref_t, val_t>
        {
            std::tuple<iterator_t<URanges>...>       its{radr::begin(uranges_)...};
            std::tuple<sentinel_t<URanges>...> const ends{radr::end(uranges_)...};

            auto at_end = [&]<size_t... I>(std::index_sequence<I...>)
            {
                return ((std::get<I>(its) == std::get<I>(ends)) || ...);
            };

            while (!at_end(std::make_index_sequence<sizeof...(uranges)>{}))
            {
                co_yield detail::tuple_transform([](auto & it) -> decltype(auto) { return *it; }, its);
                detail::tuple_for_each([](auto & it) { ++it; }, its);
            }
        }(std::move(uranges)...);
    };

    return impl(RADR_FWD(uranges_)...);
};

} // namespace cpo
} // namespace radr

#endif
