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
#include <array>
#include <cstddef>
#include <ranges>
#include <tuple>
#include <utility>

#include "radr/class/borrowing_rad.hpp"
#include "radr/class/zip_container.hpp"
#include "radr/concepts.hpp"
#include "radr/rad/take.hpp"
#include "radr/rad/transform.hpp"
#include "radr/range_access.hpp"

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

/*!\brief radr::detail::zip_policy_transform_constraints with sizeof...(Is) copies of \p UIt.
 * \details Deliberately not an immediately-invoked lambda inside the static_assert: that crashes clang 17 and 19.
 */
template <typename Fn, typename UIt, typename Seq>
inline constexpr bool zip_policy_transform_constraints_n = false;

template <typename Fn, typename UIt, size_t... Is>
inline constexpr bool zip_policy_transform_constraints_n<Fn, UIt, std::index_sequence<Is...>> =
  zip_policy_transform_constraints<Fn, pack_helper<Is, UIt>...>;

template <size_t N, typename Deref, typename UIt>
constexpr auto make_adj_it(Deref deref, std::array<UIt, N> const & arr)
{
    return [&]<size_t... Is>(std::index_sequence<Is...>)
    {
        return zip_iterator<zip_iterator_kind::adjacent, Deref, pack_helper<Is, UIt>...>{std::move(deref), arr};
    }(std::make_index_sequence<N>{});
}

template <size_t N, typename Deref, std::forward_iterator UIt, std::sentinel_for<UIt> USen>
constexpr auto make_adj_it(Deref deref, UIt uit, USen usen)
{
    return make_adj_it<N>(std::move(deref), make_adj_it_array<N>(std::move(uit), std::move(usen)));
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

    template <typename Deref, typename... Its>
    constexpr explicit adjacent_sentinel(zip_iterator<zip_iterator_kind::adjacent, Deref, Its...>, USen usen) :
      end{std::move(usen)}
    {}

    template <typename UIt2, typename USen2>
    constexpr adjacent_sentinel(adjacent_sentinel<UIt2, USen2> other)
        requires((!std::same_as<USen2, USen> || !std::same_as<UIt2, UIt>) && std::convertible_to<USen2, USen>)
      : end{std::move(other.end)}
    {}

    template <typename Deref, typename... Its>
    friend constexpr bool operator==(zip_iterator<zip_iterator_kind::adjacent, Deref, Its...> const & lhs,
                                     adjacent_sentinel const &                                        rhs)
        requires((std::same_as<UIt, Its> && ...))
    {
        return lhs.base_storage().back() == rhs.end;
    }

    template <typename Deref, typename... Its>
    friend constexpr std::iter_difference_t<UIt> operator-(
      zip_iterator<zip_iterator_kind::adjacent, Deref, Its...> const & lhs,
      adjacent_sentinel const &                                        rhs)
        requires(std::sized_sentinel_for<USen, UIt> && (std::same_as<UIt, Its> && ...))
    {
        return lhs.base_storage().back() - rhs.end;
    }

    template <typename Deref, typename... Its>
    friend constexpr std::iter_difference_t<UIt> operator-(
      adjacent_sentinel const &                                        lhs,
      zip_iterator<zip_iterator_kind::adjacent, Deref, Its...> const & rhs)
        requires(std::sized_sentinel_for<USen, UIt> && (std::same_as<UIt, Its> && ...))
    {
        return -(rhs - lhs);
    }

    constexpr USen const & base() const & { return end; }

    constexpr USen && base() && { return std::move(end); }
};

template <typename Deref, typename It1, typename... Its, typename USen>
adjacent_sentinel(zip_iterator<zip_iterator_kind::adjacent, Deref, It1, Its...>, USen) -> adjacent_sentinel<It1, USen>;

//!\brief The value type of radr::detail::adjacent_window_fn.
template <typename Deref, typename UIt, typename Seq>
struct adjacent_window_value;

template <typename Deref, typename UIt, size_t... Is>
struct adjacent_window_value<Deref, UIt, std::index_sequence<Is...>>
{
    using type = typename Deref::template value_type<pack_helper<Is, UIt>...>;
};

//!\brief The N consecutive iterators starting at \p uit, created by sequential increments.
template <size_t N, std::random_access_iterator UIt>
constexpr std::array<UIt, N> make_adj_window_its(UIt uit)
{
    return [&]<size_t... Is>(std::index_sequence<Is...>)
    {
        /* braced-init-list: evaluated left to right */
        return std::array<UIt, N>{(Is == 0 ? uit : ++uit)...};
    }(std::make_index_sequence<N>{});
}

//!\brief Creates the window from [uit, uit + N) for radr::detail::transform_it; random-access only.
template <size_t N, typename Deref>
struct adjacent_window_fn
{
    [[no_unique_address]] Deref deref{};

    template <typename UIt>
    using value_type = typename adjacent_window_value<Deref, UIt, std::make_index_sequence<N>>::type;

    template <std::random_access_iterator UIt>
    constexpr decltype(auto) operator()(UIt const & uit) const
    {
        return [&]<size_t... Is>(std::array<UIt, N> const & its, std::index_sequence<Is...>) -> decltype(auto)
        {
            return deref(its[Is]...);
        }(make_adj_window_its<N>(uit), std::make_index_sequence<N>{});
    }

    template <std::random_access_iterator UIt>
    constexpr auto iter_move(UIt const & uit) const
        requires(Deref::proxy)
    {
        return [&]<size_t... Is>(std::array<UIt, N> const & its, std::index_sequence<Is...>)
        {
            return std::tuple<pack_helper<Is, std::iter_rvalue_reference_t<UIt>>...>(
              std::ranges::iter_move(its[Is])...);
        }(make_adj_window_its<N>(uit), std::make_index_sequence<N>{});
    }

    template <std::random_access_iterator UIt>
    constexpr void iter_swap(UIt const & lhs, UIt const & rhs) const
        requires(Deref::proxy && std::indirectly_swappable<UIt>)
    {
        [&]<size_t... Is>(std::array<UIt, N> const & l, std::array<UIt, N> const & r, std::index_sequence<Is...>)
        {
            (std::ranges::iter_swap(l[Is], r[Is]), ...);
        }(make_adj_window_its<N>(lhs), make_adj_window_its<N>(rhs), std::make_index_sequence<N>{});
    }
};

//!\brief radr::detail::adjacent_borrow_impl for ranges that are not radr::safely_indexable_range.
template <ptrdiff_t N>
inline constexpr auto adjacent_borrow_impl_regular =
  []<typename Deref, std::ranges::borrowed_range URange>(Deref deref, URange && urange)
    requires std::ranges::forward_range<URange>
{
    auto beg  = make_adj_it<N>(deref, radr::begin(urange), radr::end(urange));
    auto cbeg = make_adj_it<N>(deref, radr::cbegin(urange), radr::cend(urange));

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
        /* bidi + common */
        else if constexpr (std::ranges::bidirectional_range<URange> && common_range<URange>)
        {
            return std::tuple{make_adj_it<N>(deref,
                                             std::ranges::prev(radr::end(urange), N - 1, radr::begin(urange)),
                                             radr::end(urange)),
                              make_adj_it<N>(deref,
                                             std::ranges::prev(radr::cend(urange), N - 1, radr::cbegin(urange)),
                                             radr::cend(urange))};
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

            return std::tuple{make_adj_it<N>(deref, arr), make_adj_it<N>(deref, carr)};
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

template <ptrdiff_t N>
inline constexpr auto adjacent_borrow_impl =
  []<typename Deref, std::ranges::borrowed_range URange>(Deref deref, URange && urange)
    requires std::ranges::forward_range<URange>
{
    static_assert(N > 0, "You must select N > 0 for radr::adjacent.");

    /* Special case: one underlying iterator; the window is created on dereference */
    if constexpr (safely_indexable_range<URange>)
    {
        adjacent_window_fn<N, Deref> fn{std::move(deref)};

        if constexpr (std::ranges::sized_range<URange>)
        {
            auto usize = std::ranges::size(urange);
            auto n     = static_cast<decltype(usize)>(N);
            return transform_borrow<false>(take_borrow(urange, usize >= n ? usize - n + 1 : 0), std::move(fn));
        }
        else // infinite_mp_range
        {
            return transform_borrow<false>(std::forward<URange>(urange), std::move(fn));
        }
    }
    else
    {
        return adjacent_borrow_impl_regular<N>(std::move(deref), std::forward<URange>(urange));
    }
};

//!\brief radr::detail::adjacent_borrow_impl with the plain zip policy.
template <ptrdiff_t N>
inline constexpr auto adjacent_borrow = []<std::ranges::borrowed_range URange>(URange && urange)
    requires std::ranges::forward_range<URange>
{
    return adjacent_borrow_impl<N>(zip_policy_tuple{}, std::forward<URange>(urange));
};

} // namespace radr::detail
