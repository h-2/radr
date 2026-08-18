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

#include "radr/rad_util/rad_interface.hpp"
#include "radr/version.hpp"

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include <algorithm>
#    include <concepts>
#    include <functional>
#    include <iterator>
#    include <ranges>
#    include <type_traits>
#    include <utility>

#    include "../concepts.hpp"
#    include "radr/custom/tags.hpp"
#    include "radr/detail/detail.hpp"
#    include "radr/factory/iota.hpp"
#    include "radr/rad/reverse.hpp"
#    include "radr/range_access.hpp"

namespace radr::detail
{

template <class F, class Tuple>
constexpr auto tuple_transform(F && f, Tuple && tuple)
{
    return std::apply([&]<class... Ts>(Ts &&... args)
    { return std::tuple<std::invoke_result_t<F &, Ts>...>(std::invoke(f, std::forward<Ts>(args))...); },
                      std::forward<Tuple>(tuple));
}

template <class F, class Tuple1, class Tuple2>
constexpr auto tuple_zip_transform(F && f, Tuple1 && tuple1, Tuple2 && tuple2)
{
    auto impl = []<size_t... I>(F & f, Tuple1 && tuple1, Tuple2 && tuple2, std::index_sequence<I...>)
      -> std::tuple<std::invoke_result_t<F &,
                                         std::tuple_element_t<I, std::remove_cvref_t<Tuple1>>,
                                         std::tuple_element_t<I, std::remove_cvref_t<Tuple2>>>...>
    {
        return {f(std::get<I>(std::forward<Tuple1>(tuple1)), std::get<I>(std::forward<Tuple2>(tuple2)))...};
    };

    return impl(f,
                std::forward<Tuple1>(tuple1),
                std::forward<Tuple2>(tuple2),
                std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<Tuple1>>>{});
}

template <class F, class Tuple>
constexpr void tuple_for_each(F && f, Tuple && tuple)
{
    std::apply([&]<class... Ts>(Ts &&... args) { (static_cast<void>(std::invoke(f, std::forward<Ts>(args))), ...); },
               std::forward<Tuple>(tuple));
}

//!\brief Relevant for rebind-behaviour on deep-copies and specialised compare.
enum class zip_iterator_kind
{
    container, //!< used by zip_rng / zip factory (owning container); no rebind because always at "source" of pipe
    adaptor,   //!< used by zip_with adaptor; rebinds first (only range that can be owning)
    enumerate, //!< used by enumerate adaptor; rebinds second (because that's the underlying range in enumerate)
    adjacent   //!< used by adjacent adaptor; rebinds first, re-derives the rest (all point into the same range)
};

template <typename... Args>
class zip_sentinel;

template <typename UIt, typename USen>
class enumerate_sentinel;

template <typename UIt, typename USen>
class adjacent_sentinel;

template <zip_iterator_kind kind, typename... UIt>
    requires((std::forward_iterator<UIt> && ...))
class zip_iterator
{
    static constexpr size_t _size = sizeof...(UIt);
    static_assert(_size > 0, "There must be > 0 template arguments to zip_iterator.");

    using first_uit_t               = pack_head_t<UIt...>;
    static constexpr bool _all_same = (std::same_as<UIt, first_uit_t> && ...);

    static_assert((kind != zip_iterator_kind::adjacent) || _all_same,
                  "Adjacent means all iterator types are the same.");

    template <zip_iterator_kind kind2, typename... UIt2>
        requires((std::forward_iterator<UIt2> && ...))
    friend class zip_iterator;

    template <typename... Args2>
    friend class zip_sentinel;

    template <typename UIt2, typename USen2>
    friend class enumerate_sentinel;

    template <typename UIt2, typename USen2>
    friend class adjacent_sentinel;

    // this overload is only injected for the adaptors, not the container
    template <typename Container>
        requires(kind != zip_iterator_kind::container)
    constexpr friend auto tag_invoke(custom::rebind_iterator_tag,
                                     zip_iterator it,
                                     Container &  container_old,
                                     Container &  container_new)
    {
        /* adjacent holds N iterators into the *same* range, so all of them are stale after a rebind.
         * To avoid multiple rebinds, we rebind the first and then recreate the others from that one.
         */
        if constexpr (kind == zip_iterator_kind::adjacent)
        {
            // auto new_it = tag_invoke(custom::rebind_iterator_tag{}, it.current[0], container_old, container_new);
            // it.current  = make_adj_it_array<_size>(new_it, radr::end(container_new));

            auto oldarr = it.current;

            it.current[0] = tag_invoke(custom::rebind_iterator_tag{}, it.current[0], container_old, container_new);
            for (size_t i = 1; i < _size; ++i)
                it.current[i] = (oldarr[i] == oldarr[i - 1]) ? it.current[i - 1] : std::next(it.current[i - 1]);
        }
        else
        {
            // enumerate switches out the 1st elem not the 0th elem (because that's the number)
            static constexpr size_t elem_i = (kind == zip_iterator_kind::enumerate) ? 1 : 0;
            std::get<elem_i>(it.current) =
              tag_invoke(custom::rebind_iterator_tag{}, std::get<elem_i>(it.current), container_old, container_new);
        }
        return it;
    }

    auto & get_element_for_compare() const
    {
        // only need to compare 0-th element
        if constexpr (kind == zip_iterator_kind::enumerate || kind == zip_iterator_kind::adjacent)
            return std::get<0>(current);
        else
            return current; // compare everything by default
    }

    static constexpr bool is_random_access = (std::random_access_iterator<UIt> && ...);
    static constexpr bool is_bidi          = (std::bidirectional_iterator<UIt> && ...);

    /* data members */
    using storage_type = std::conditional_t<_all_same, std::array<first_uit_t, _size>, std::tuple<UIt...>>;
    storage_type current;

public:
    // clang-format off
    using iterator_concept = std::conditional_t<is_random_access, std::random_access_iterator_tag,
                             std::conditional_t<is_bidi,          std::bidirectional_iterator_tag,
                                                                  std::forward_iterator_tag>>;
    // clang-format on

    using value_type      = std::tuple<std::iter_value_t<UIt>...>;
    using difference_type = std::common_type_t<std::iter_difference_t<UIt>...>;

    zip_iterator() = default;

    constexpr zip_iterator(UIt... uit)
        requires(!_all_same)
      : current{std::move(uit)...}
    {}

    constexpr zip_iterator(UIt... uit)
        requires(_all_same)
    {
        // tuple2array
        [&](auto... args)
        {
            size_t i = 0;
            ((current[i++] = std::move(args)), ...);
        }(std::move(uit)...);
    }

    constexpr zip_iterator(std::array<first_uit_t, _size> const & arr)
        requires(_all_same)
      : current{std::move(arr)}
    {}

    template <typename... UIt2>
    constexpr zip_iterator(zip_iterator<kind, UIt2...> other)
        requires((!std::same_as<UIt2, UIt> || ...) && (std::convertible_to<UIt2, UIt> && ...) &&
                 sizeof...(UIt2) == _size && zip_iterator<kind, UIt2...>::_all_same == _all_same)
    {
        if constexpr (_all_same)
            std::ranges::move(other.current, current.data());
        else
            current = std::move(other.current);
    }

    constexpr auto operator*() const
    {
        return tuple_transform([](auto & it) -> decltype(auto) { return *it; }, current);
    }

    constexpr zip_iterator & operator++()
    {
        if constexpr (_all_same)
        {
            for (auto & it : current)
                ++it;
        }
        else
        {
            tuple_for_each([](auto & it) { return ++it; }, current);
        }
        return *this;
    }

    constexpr zip_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr zip_iterator & operator--()
        requires is_bidi
    {
        if constexpr (_all_same)
        {
            for (auto & it : current)
                --it;
        }
        else
        {
            tuple_for_each([](auto & it) { return --it; }, current);
        }
        return *this;
    }

    constexpr zip_iterator operator--(int)
        requires is_bidi
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr zip_iterator & operator+=(difference_type n)
        requires is_random_access
    {
        tuple_for_each([n]<typename It>(It & it) { return it += static_cast<std::iter_difference_t<It>>(n); }, current);
        return *this;
    }

    constexpr zip_iterator & operator-=(difference_type n)
        requires is_random_access
    {
        tuple_for_each([n]<typename It>(It & it) { return it -= static_cast<std::iter_difference_t<It>>(n); }, current);
        return *this;
    }

    constexpr decltype(auto) operator[](difference_type n) const
        requires is_random_access
    {
        return tuple_transform([n]<typename It>(It & it) -> decltype(auto)
        { return it[static_cast<std::iter_difference_t<It>>(n)]; },
                               current);
    }

    friend constexpr bool operator==(zip_iterator const & lhs, zip_iterator const & rhs)
    {
        return lhs.get_element_for_compare() == rhs.get_element_for_compare();
    }

    friend constexpr bool operator<(zip_iterator const & lhs, zip_iterator const & rhs)
        requires is_random_access
    {
        return lhs.get_element_for_compare() < rhs.get_element_for_compare();
    }

    friend constexpr bool operator>(zip_iterator const & lhs, zip_iterator const & rhs)
        requires is_random_access
    {
        return lhs.get_element_for_compare() > rhs.get_element_for_compare();
    }

    friend constexpr bool operator<=(zip_iterator const & lhs, zip_iterator const & rhs)
        requires is_random_access
    {
        return lhs.get_element_for_compare() <= rhs.get_element_for_compare();
    }

    friend constexpr bool operator>=(zip_iterator const & lhs, zip_iterator const & rhs)
        requires is_random_access
    {
        return lhs.get_element_for_compare() >= rhs.get_element_for_compare();
    }

    friend constexpr auto operator<=>(zip_iterator const & lhs, zip_iterator const & rhs)
        requires is_random_access && (std::three_way_comparable<UIt> && ...)
    {
        return lhs.get_element_for_compare() <=> rhs.get_element_for_compare();
    }

    friend constexpr zip_iterator operator+(zip_iterator it, difference_type n)
        requires is_random_access
    {
        it += n;
        return it;
    }

    friend constexpr zip_iterator operator+(difference_type n, zip_iterator it)
        requires is_random_access
    {
        it += n;
        return it;
    }

    friend constexpr zip_iterator operator-(zip_iterator it, difference_type n)
        requires is_random_access
    {
        it -= n;
        return it;
    }

    friend constexpr difference_type operator-(zip_iterator const & lhs, zip_iterator const & rhs)
        requires(std::sized_sentinel_for<UIt, UIt> && ...)
    {
        return [&]<size_t... I>(std::index_sequence<I...>)
        {
            return std::ranges::min(
              {static_cast<difference_type>(std::get<I>(lhs.current) - std::get<I>(rhs.current))...});
        }(std::make_index_sequence<sizeof...(UIt)>{});
    }

    friend constexpr decltype(auto) iter_move(zip_iterator const & i) noexcept(
      (noexcept(std::ranges::iter_move(UIt{})) && ...) &&
      (std::is_nothrow_move_constructible_v<std::iter_rvalue_reference_t<UIt>> && ...))
    {
        return tuple_transform(std::ranges::iter_move, i.current);
    }

    friend constexpr void iter_swap(zip_iterator const & lhs, zip_iterator const & rhs) noexcept(
      (noexcept(std::ranges::iter_swap(UIt{}, UIt{})) && ...))
        requires((std::indirectly_swappable<UIt> && ...))
    {
        [&]<size_t... I>(std::index_sequence<I...>)
        {
            (std::ranges::iter_swap(std::get<I>(lhs.current), std::get<I>(rhs.current)), ...);
        }(std::make_index_sequence<sizeof...(UIt)>{});
    }
};

template <typename... UIt>
zip_iterator(UIt...) -> zip_iterator<zip_iterator_kind::adaptor, UIt...>;

template <zip_iterator_kind k, typename... UIt>
constexpr auto make_zip_it(UIt... uit)
{
    return zip_iterator<k, UIt...>{std::forward<UIt>(uit)...};
}

/*!\brief Sentinel type for Zip adaptors.
 * \details This is only used for zip_iterator_kind::adaptor! The container does not use it, and enumerate and adjacent
 * have their own sentinels
 */
template <typename... UIt, typename... USen>
class zip_sentinel<std::tuple<UIt...>, std::tuple<USen...>>
{
    static_assert(sizeof...(UIt) == sizeof...(USen), "Template arguments to zip_sentinel don't have same length.");
    static_assert(sizeof...(UIt) > 0, "There must be > 0 template arguments to zip_sentinel.");
    static_assert((std::sentinel_for<USen, UIt> && ...),
                  "zip_sentinel's sentinel types must be sentinels for the iterator types.");

    //TODO array special case?
    [[no_unique_address]] std::tuple<USen...> end{};

    template <typename... Args>
    friend class zip_sentinel;

    // this overload is only injected for the adaptors, not the container
    template <typename Container>
    constexpr friend auto tag_invoke(custom::rebind_iterator_tag,
                                     zip_sentinel it,
                                     Container &  container_old,
                                     Container &  container_new)
    {
        std::get<0>(it.end) =
          tag_invoke(custom::rebind_iterator_tag{}, std::get<0>(it.end), container_old, container_new);
        return it;
    }

public:
    zip_sentinel() = default;

    template <zip_iterator_kind _>
    constexpr explicit zip_sentinel(zip_iterator<_, UIt...>, std::tuple<USen...> usens) : end{std::move(usens)}
    {}

    template <typename... UIt2, typename... USen2>
    constexpr zip_sentinel(zip_sentinel<std::tuple<UIt2...>, std::tuple<USen2...>> other)
        requires(((!std::same_as<USen2, USen> || ...) || (!std::same_as<UIt2, UIt> || ...)) &&
                 (std::convertible_to<USen2, USen> && ...))
      : end{std::move(other.end)}
    {}

    template <zip_iterator_kind k>
    friend constexpr bool operator==(zip_iterator<k, UIt...> const & lhs, zip_sentinel const & rhs)
    {
        return [&]<size_t... I>(std::index_sequence<I...>)
        {
            return ((std::get<I>(lhs.current) == std::get<I>(rhs.end)) || ...);
        }(std::make_index_sequence<sizeof...(UIt)>{});
    }

    template <zip_iterator_kind k>
    friend constexpr std::iter_difference_t<zip_iterator<k, UIt...>> operator-(zip_iterator<k, UIt...> const & lhs,
                                                                               zip_sentinel const &            rhs)
        requires((std::sized_sentinel_for<USen, UIt> && ...))
    {
        constexpr auto diff = [](auto && lhs, auto && rhs)
        {
            return lhs - rhs;
        };
        constexpr auto pack_min = [](auto... values)
        {
            return std::ranges::min({values...});
        };
        return std::apply(pack_min, detail::tuple_zip_transform(diff, lhs.current, rhs.end));
    }

    template <zip_iterator_kind k>
    friend constexpr std::iter_difference_t<zip_iterator<k, UIt...>> operator-(zip_sentinel const &            lhs,
                                                                               zip_iterator<k, UIt...> const & rhs)
        requires((std::sized_sentinel_for<USen, UIt> && ...))
    {
        return -(rhs - lhs);
    }
};

template <zip_iterator_kind k, typename... UIt, typename... USen>
zip_sentinel(zip_iterator<k, UIt...>, std::tuple<USen...>) -> zip_sentinel<std::tuple<UIt...>, std::tuple<USen...>>;

} // namespace radr::detail

namespace radr
{

/*!\brief A container of multiple other containers.
 * \tparam URanges The underlying ranges.
 *
 * All underlying ranges must be cv-unqualified object types that model radr::mp_range.
 * At least one underlying range must be a container (i.e. not be borrowed).
 *
 * To zip over multiple borrowed ranges, use the radr::zip-factory or the radr::zip_with-adaptor. Both
 * will return a radr::borrowing_rad.
 */
template <typename... URanges>
class zip_rng : public rad_interface<zip_rng<URanges...>>
{
private:
    static_assert(sizeof...(URanges) > 0, "There must be > 0 template arguments to zip_rng.");
    static_assert((range_object<URanges> && ...), "All arguments to zip_rng must be unqualified multi-passed ranges.");
    static_assert(((!borrowed_mp_range<URanges>) || ...),
                  "If all argument to zip_rng are borrowed, use radr::borrowed_rad instead.");

public:
    using iterator       = detail::zip_iterator<detail::zip_iterator_kind::container, iterator_t<URanges>...>;
    using const_iterator = detail::zip_iterator<detail::zip_iterator_kind::container, const_iterator_t<URanges>...>;

private:
    static constexpr bool is_ra_sized =
      (safely_indexable_range<URanges const> && ...) && (std::ranges::sized_range<URanges const> || ...);
    static constexpr bool is_sized = (std::ranges::sized_range<URanges const> && ...) || is_ra_sized;
    static constexpr bool const_symmetric =
      std::same_as<std::iter_reference_t<iterator>, std::iter_reference_t<const_iterator>>;

    using size_type =
      std::conditional_t<is_sized, std::common_type_t<detail::range_size_t_or_size_t<URanges>...>, detail::not_size>;

    std::tuple<URanges...> containers;
    size_type              sz{}; // stored size

    template <typename iterator_t, typename get_end_t, typename self_t>
    static auto end_impl(self_t & self)
    {
        // common via random access
        if constexpr (is_ra_sized)
        {
            return self.begin() + self.size();
        }
        else
        {
            constexpr auto make_zip_iterator = [](auto &&... rngs)
            {
                static constexpr get_end_t get_end{};

                // common only if end can be deduced in o(1)
                if constexpr ((common_range<URanges const> && ...) &&
                              (sizeof...(URanges) == 1 || !std::bidirectional_iterator<const_iterator>))
                {
                    return iterator_t{get_end(rngs)...};
                }
                else // not common
                {
                    return detail::zip_sentinel{iterator_t{}, std::make_tuple(get_end(rngs)...)};
                }
            };
            return std::apply(make_zip_iterator, self.containers);
        }
    }

public:
    using value_type      = std::iter_value_t<iterator>;
    using difference_type = std::iter_difference_t<iterator>;

    /*!\name Constructors: Rule-of-5
     * \{
     */
    constexpr zip_rng()                            = default;
    constexpr zip_rng(zip_rng const &)             = default;
    constexpr zip_rng(zip_rng &&)                  = default;
    constexpr zip_rng & operator=(zip_rng const &) = default;
    constexpr zip_rng & operator=(zip_rng &&)      = default;

    constexpr zip_rng(URanges &&... uranges) :
      containers{std::make_tuple(std::forward<URanges>(uranges)...)},
      sz{std::apply(detail::min_range_weak_size, containers)}
    {}
    //!\}

    constexpr iterator begin()
        requires(!const_symmetric)
    {
        auto make_zip_iterator = [](auto &&... rngs)
        {
            return iterator{radr::begin(rngs)...};
        };
        return std::apply(make_zip_iterator, containers);
    }

    constexpr const_iterator begin() const
    {
        auto make_zip_iterator = [](auto &&... rngs)
        {
            return const_iterator{radr::cbegin(rngs)...};
        };
        return std::apply(make_zip_iterator, containers);
    }

    constexpr auto end()
        requires(!const_symmetric)
    {
        return end_impl<iterator, decltype(radr::end)>(*this);
    }

    constexpr auto end() const { return end_impl<const_iterator, decltype(radr::cend)>(*this); }

    auto size() const
        requires is_sized
    {
        return sz;
    }
};

template <typename... URanges>
zip_rng(URanges &&...) -> zip_rng<std::remove_cvref_t<URanges>...>;

} // namespace radr

#endif
