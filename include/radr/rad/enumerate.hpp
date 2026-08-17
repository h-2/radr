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

#include "../version.hpp"
#include "radr/class/zip_rng.hpp"

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include <ranges>

#    include "../concepts.hpp"
#    include "../detail/detail.hpp"
#    include "../detail/pipe.hpp"
#    include "../factory/iota.hpp"
#    include "../factory/zip.hpp"

namespace radr::detail
{

template <typename UIt, typename USen>
class enumerate_sentinel
{
    [[no_unique_address]] USen end{};

public:
    enumerate_sentinel() = default;

    constexpr explicit enumerate_sentinel(USen usen) : end{std::move(usen)} {}

    template <typename UIt2, typename USen2>
    constexpr enumerate_sentinel(enumerate_sentinel<UIt2, USen2> other)
        requires((!std::same_as<USen2, USen>) && std::convertible_to<USen2, USen>)
      : end{std::move(other.end)}
    {}

    template <typename IotaIt>
    friend constexpr bool operator==(zip_iterator<zip_iterator_kind::enumerate, IotaIt, UIt> const & lhs,
                                     enumerate_sentinel const &                                      rhs)
    {
        return std::get<1>(lhs.current) == rhs.end;
    }

    template <typename IotaIt>
    friend constexpr std::iter_difference_t<UIt> operator-(
      zip_iterator<zip_iterator_kind::enumerate, IotaIt, UIt> const & lhs,
      enumerate_sentinel const &                                      rhs)
        requires std::sized_sentinel_for<USen, UIt>
    {
        return std::get<1>(lhs.current) - rhs.end;
    }

    template <typename IotaIt>
    friend constexpr std::iter_difference_t<UIt> operator-(
      enumerate_sentinel const &                                      lhs,
      zip_iterator<zip_iterator_kind::enumerate, IotaIt, UIt> const & rhs)
        requires std::sized_sentinel_for<USen, UIt>
    {
        return -(rhs - lhs);
    }
};

inline constexpr auto enumerate_borrow = []<std::ranges::borrowed_range URange>(URange && urange)
    requires std::ranges::forward_range<URange>
{
    using diff_t = std::ranges::range_difference_t<URange>;

    auto [iota_range, s] = [](auto & _urange)
    {
        if constexpr (std::ranges::sized_range<URange>)
        {
            auto s  = std::ranges::size(_urange);
            auto io = radr::iota(diff_t{0}, static_cast<diff_t>(s));
            return std::make_tuple(std::move(io), s);
        }
        else
        {
            auto io = radr::iota(diff_t{0});
            return std::make_tuple(std::move(io), not_size{});
        }
    }(urange);

    auto beg  = make_zip_it<zip_iterator_kind::enumerate>(radr::begin(iota_range), radr::begin(urange));
    auto cbeg = make_zip_it<zip_iterator_kind::enumerate>(radr::cbegin(iota_range), radr::cbegin(urange));

    /* infinite */
    if constexpr (infinite_mp_range<URange>)
    {
        return borrowing_rad{beg, std::unreachable_sentinel, cbeg, std::unreachable_sentinel};
    }
    /*  RA+sized */
    else if constexpr (std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
    {
        auto end  = beg + s;
        auto cend = cbeg + s;

        return borrowing_rad{beg, end, cbeg, cend, s};
    }
    /* common */
    else if constexpr (common_range<URange> && std::ranges::sized_range<URange>)
    {
        auto end  = make_zip_it<zip_iterator_kind::enumerate>(radr::end(iota_range), radr::end(urange));
        auto cend = make_zip_it<zip_iterator_kind::enumerate>(radr::cend(iota_range), radr::cend(urange));

        return borrowing_rad{beg, end, cbeg, cend, s};
    }
    /* all other cases */
    else
    {
        auto end  = enumerate_sentinel<iterator_t<URange>, sentinel_t<URange>>{radr::end(urange)};
        auto cend = enumerate_sentinel<const_iterator_t<URange>, const_sentinel_t<URange>>{radr::cend(urange)};

        return borrowing_rad{beg, end, cbeg, cend, s};
    }
};

inline constexpr auto enumerate_coro = []<std::ranges::input_range URange>(URange && urange)
{
    static_assert(!container_lvalue<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    using diff_t = std::ranges::range_difference_t<URange>;
    return radr::zip_sp(radr::iota_sp(diff_t{0}), std::move(urange));
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Enumerate a range, yielding (index, value) pairs.
 * \param urange The underlying range.
 *
 * ### Multi-pass adaptor
 *
 * Requirements on \p urange :
 *   * radr::mp_range
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::borrowed_range
 *   * std::ranges::sized_range
 *   * radr::common_range (but only if also sized)
 *   * radr::constant_range
 *   * radr::mutable_range (although the index element is always read-only)
 *
 * ### Single-pass adaptor
 *
 * Requirements on \p urange :
 *   * std::ranges::input_range
 *
 * Returns a radr::generator of `std::tuple<range_difference_t, std::ranges::ranges_reference_t<URange>>`.
 */
inline constexpr auto enumerate = detail::pipe_without_args_fn{detail::enumerate_coro, detail::enumerate_borrow};
} // namespace cpo
} // namespace radr

#endif
