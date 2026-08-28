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

#include "radr/version.hpp"

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include <algorithm>
#    include <array>
#    include <ranges>

#    include "radr/class/zip_rng.hpp"
#    include "radr/concepts.hpp"
#    include "radr/detail/pipe.hpp"

namespace radr::detail
{

/*!\brief Create the internal array type for an adjacent iterator.
 * \details
 * Typically, the returned array will contains N adjacent iterators, but there is a special case:
 * If `N > distance(it, sen)`, all elements after distance(it, sen) need to be equal to the last.
 *
 * This is because the at-end-check looks at the last element in the array.
 */
template <ptrdiff_t N, typename It, typename Sen>
constexpr std::array<It, N> make_adj_it_array(It it, Sen sen)
{
    /* the offsets are known up front; no sentinel comparisons and no serial increments */
    if constexpr (std::random_access_iterator<It> && std::sized_sentinel_for<Sen, It>)
    {
        using diff_t = std::iter_difference_t<It>;
        diff_t s     = sen - it;
        return [&]<size_t... Is>(std::index_sequence<Is...>)
        {
            /* std::min keeps us within [it, sen], i.e. ranges shorter than N collapse onto the end */
            return std::array<It, N>{(it + std::min<diff_t>(Is, s))...};
        }(std::make_index_sequence<N>{});
    }
    else
    {
        std::array<It, N> ret{};

        for (It & r : ret)
            r = (it == sen) ? it : it++;

        return ret;
    }
}

template <size_t, typename T>
using pack_helper = T;

template <size_t N, typename UIt>
constexpr auto make_adj_it(std::array<UIt, N> const & arr)
{
    return [&arr]<size_t... Is>(std::index_sequence<Is...>)
    {
        return zip_iterator<zip_iterator_kind::adjacent, pack_helper<Is, UIt>...>{arr};
    }(std::make_index_sequence<N>{});
}

template <size_t N, std::forward_iterator UIt, std::sentinel_for<UIt> USen>
constexpr auto make_adj_it(UIt uit, USen usen)
{
    return make_adj_it(make_adj_it_array<N>(std::move(uit), std::move(usen)));
}

template <typename UIt, typename USen>
class adjacent_sentinel
{
    [[no_unique_address]] USen end{};

    template <typename UIt2, typename USen2>
    friend class adjacent_sentinel;

public:
    constexpr adjacent_sentinel() = default;

    constexpr explicit adjacent_sentinel(USen usen) : end{std::move(usen)} {}

    template <typename... Its>
    constexpr explicit adjacent_sentinel(zip_iterator<zip_iterator_kind::adjacent, Its...>, USen usen) :
      end{std::move(usen)}
    {}

    template <typename UIt2, typename USen2>
    constexpr adjacent_sentinel(adjacent_sentinel<UIt2, USen2> other)
        requires((!std::same_as<USen2, USen> || !std::same_as<UIt2, UIt>) && std::convertible_to<USen2, USen>)
      : end{std::move(other.end)}
    {}

    template <typename... Its>
    friend constexpr bool operator==(zip_iterator<zip_iterator_kind::adjacent, Its...> const & lhs,
                                     adjacent_sentinel const &                                 rhs)
        requires((std::same_as<UIt, Its> && ...))
    {
        return lhs.current.back() == rhs.end;
    }

    template <typename... Its>
    friend constexpr std::iter_difference_t<UIt> operator-(
      zip_iterator<zip_iterator_kind::adjacent, Its...> const & lhs,
      adjacent_sentinel const &                                 rhs)
        requires(std::sized_sentinel_for<USen, UIt> && (std::same_as<UIt, Its> && ...))
    {
        return lhs.current.back() - rhs.end;
    }

    template <typename... Its>
    friend constexpr std::iter_difference_t<UIt> operator-(
      adjacent_sentinel const &                                 lhs,
      zip_iterator<zip_iterator_kind::adjacent, Its...> const & rhs)
        requires(std::sized_sentinel_for<USen, UIt> && (std::same_as<UIt, Its> && ...))
    {
        return -(rhs - lhs);
    }

    constexpr USen const & base() const & { return end; }

    constexpr USen && base() && { return std::move(end); }
};

template <typename It1, typename... Its, typename USen>
adjacent_sentinel(zip_iterator<zip_iterator_kind::adjacent, It1, Its...>, USen) -> adjacent_sentinel<It1, USen>;

template <ptrdiff_t N>
inline constexpr auto adjacent_borrow = []<std::ranges::borrowed_range URange>(URange && urange)
    requires std::ranges::forward_range<URange>
{
    static_assert(N > 0, "You must select N > 0 for radr::adjacent.");

    auto beg  = make_adj_it<N>(radr::begin(urange), radr::end(urange));
    auto cbeg = make_adj_it<N>(radr::cbegin(urange), radr::cend(urange));

    using diff_t = std::common_type_t<std::ranges::range_difference_t<URange>, ptrdiff_t>;

    auto const s = [](auto & _urange)
    {
        if constexpr (std::ranges::sized_range<URange>)
        {
            diff_t s = std::ranges::ssize(_urange);
            s -= std::min<diff_t>(s, static_cast<diff_t>(N) - 1);
            return static_cast<std::make_unsigned_t<diff_t>>(s);
        }
        else
        {
            return not_size{};
        }
    }(urange);

    auto [end, cend] = [&]()
    {
        /* infinite → result infinite */
        if constexpr (infinite_mp_range<URange>)
        {
            return std::tuple{std::unreachable_sentinel, std::unreachable_sentinel};
        }
        /* RA + sized*/
        else if constexpr (std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
        {
            return std::tuple{beg + s, cbeg + s};
        }
        /* bidi + common */
        else if constexpr (std::ranges::bidirectional_range<URange> && common_range<URange>)
        {
            return std::tuple{
              make_adj_it<N>(std::ranges::prev(radr::end(urange), N - 1, radr::begin(urange)), radr::end(urange)),
              make_adj_it<N>(std::ranges::prev(radr::cend(urange), N - 1, radr::cbegin(urange)), radr::cend(urange))};
        }
        /* uni + common */
        else if constexpr (common_range<URange>)
        {
            /* we don't have operator--, so all elements are set to the end.
             * → The first entries of the arr are "incorrect".
             * To still achieve correct behaviour, the comparison in zip_rng uses the
             * last element for unidirectional iterators. See get_element_for_compare.
             */
            std::array<iterator_t<URange>, N> arr;
            arr.fill(radr::end(urange));

            std::array<const_iterator_t<URange>, N> carr;
            carr.fill(radr::cend(urange));

            return std::tuple{make_adj_it<N>(arr), make_adj_it<N>(carr)};
        }
        /* all other cases */
        else
        {
            return std::tuple{
              adjacent_sentinel{ beg,  radr::end(urange)},
              adjacent_sentinel{cbeg, radr::cend(urange)}
            };
        }
    }();

    return borrowing_rad{beg, end, cbeg, cend, s};
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Create a sliding tuple of fixed size N over the range.
 * \tparam N The size of the tuple (>= 1).
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
 *   * radr::common_range
 *   * radr::constant_range
 *   * radr::mutable_range
 *
 * ### Single-pass adaptor
 *
 * radr::adjacent cannot be created on single-pass ranges.
 */
template <ptrdiff_t N>
inline constexpr auto adjacent = detail::pipe_without_args_fn<void, decltype(detail::adjacent_borrow<N>)>{};
} // namespace cpo
} // namespace radr

#endif
