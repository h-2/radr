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
#include <concepts>
#include <functional>
#include <iterator>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "radr/concepts.hpp"
#include "radr/custom/tags.hpp"
#include "radr/detail/detail.hpp"
#include "radr/detail/semiregular_box.hpp"
#include "radr/range_access.hpp"

/* This is the machinery for zip-based ranges, including
 *  class:      zip_rng
 *  factory:    zip
 *  adaptors:   zip_with, enumerate, adjacent, pairwise
 *
 * And also for the _transform variants:
 *  factory:    zip_transform
 *  adaptors:   zip_with_transform, adjacent_transform, pairwise_transform
 *
 * The machinery itself does not require C++23, but all entities in the first paragraph do,
 * because they have tuple-of-reference as the range reference type. This is only supported
 * with P2321R2 (C++23).
 *
 * The *_transform variants are fine in C++20.
 */

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

/*!\brief Dereference policy of radr::detail::zip_iterator yielding a std::tuple of references.
 * \details Used by radr::zip, radr::zip_with, radr::adjacent and radr::enumerate. The tuple value_type
 * restricts instantiations to C++23; see the note at the top of this header.
 */
struct zip_deref
{
    //!\brief Proxy reference; selects the tuple-flavoured iter_move/iter_swap.
    static constexpr bool proxy = true;

    template <typename... UIt>
    using value_type = std::tuple<std::iter_value_t<UIt>...>;

    template <typename... UIt>
    constexpr auto operator()(UIt const &... its) const
    {
        return std::tuple<std::iter_reference_t<UIt>...>(*its...);
    }
};

/*!\brief Dereference policy of radr::detail::zip_iterator invoking an N-ary functor.
 * \details Used by the *_transform adaptors. No std::tuple appears in any associated type, so
 * instantiations are C++20.
 */
template <typename Fn>
struct transform_deref
{
    //!\brief No proxy; iter_move follows radr::detail::transform_iterator and iter_swap is dropped.
    static constexpr bool proxy = false;

    [[no_unique_address]] semiregular_box<Fn> fn{};

    template <typename... UIt>
    using value_type = std::remove_cvref_t<std::invoke_result_t<Fn const &, std::iter_reference_t<UIt>...>>;

    template <typename... UIt>
    constexpr decltype(auto) operator()(UIt const &... its) const
    {
        return std::invoke(*fn, *its...);
    }
};

//!\brief Requirements of radr::detail::transform_deref on \p Fn, for one pack of underlying iterators.
template <typename Fn, typename... UIts>
concept transform_deref_constraints =
  std::is_object_v<Fn> && std::regular_invocable<Fn const &, std::iter_reference_t<UIts>...> &&
  can_reference<std::invoke_result_t<Fn const &, std::iter_reference_t<UIts>...>>;

template <typename... Args>
class zip_sentinel;

template <typename UIt, typename USen>
class enumerate_sentinel;

template <typename UIt, typename USen>
class adjacent_sentinel;

template <zip_iterator_kind kind, typename Deref, typename... UIt>
    requires((std::forward_iterator<UIt> && ...))
class zip_iterator
{
    static constexpr size_t _size = sizeof...(UIt);
    static_assert(_size > 0, "There must be > 0 template arguments to zip_iterator.");

    using first_uit_t               = pack_head_t<UIt...>;
    static constexpr bool _all_same = (std::same_as<UIt, first_uit_t> && ...);

    static_assert((kind != zip_iterator_kind::adjacent) || _all_same,
                  "Adjacent means all iterator types are the same.");

    template <zip_iterator_kind kind2, typename Deref2, typename... UIt2>
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
            // NOTE: not `static constexpr`, because that is C++23-only inside a constexpr function
            constexpr size_t elem_i = (kind == zip_iterator_kind::enumerate) ? 1 : 0;
            std::get<elem_i>(it.current) =
              tag_invoke(custom::rebind_iterator_tag{}, std::get<elem_i>(it.current), container_old, container_new);
        }
        return it;
    }

    auto & get_element_for_compare() const
    {
        /* enumerate: only compare the 0-th element (the counter!) */
        if constexpr (kind == zip_iterator_kind::enumerate)
            return std::get<0>(current);
        /* adjacent
         * all N iterators move in lockstep, so it suffices to compare one of them
         *
         * std::adjacent_view compares the last, but benchmarks have shown it to be
         * favourable to compare the first, especially for RA+sized ranges and N > 4,
         * so that's what we do by default.
         *
         * There is one exception: for uni-directional+common ranges, the sentinel is
         * initialised as an array{end, end, end} (because we don't have operator--
         * to decrement the first entries).
         * In this case, we need to compare the last element.
         */
        else if constexpr (kind == zip_iterator_kind::adjacent)
        {
            if constexpr (is_bidi)
                return std::get<0>(current);
            else
                return current.back();
        }
        else
            return current; // compare everything by default
    }

    static constexpr bool is_random_access = (std::random_access_iterator<UIt> && ...);
    static constexpr bool is_bidi          = (std::bidirectional_iterator<UIt> && ...);

    /* data members */
    using storage_type = std::conditional_t<_all_same, std::array<first_uit_t, _size>, std::tuple<UIt...>>;

    [[no_unique_address]] Deref deref_{};
    storage_type                current;

public:
    // clang-format off
    using iterator_concept = std::conditional_t<is_random_access, std::random_access_iterator_tag,
                             std::conditional_t<is_bidi,          std::bidirectional_iterator_tag,
                                                                  std::forward_iterator_tag>>;
    // clang-format on

    using value_type      = typename Deref::template value_type<UIt...>;
    using difference_type = std::common_type_t<std::iter_difference_t<UIt>...>;

    zip_iterator() = default;

    /* Policy-less constructors default-construct deref_; restricted to zip_deref, because
     * transform_deref<Fn> would hold an empty semiregular_box for non-default-constructible Fn. */
    constexpr zip_iterator(UIt... uit)
        requires(Deref::proxy && !_all_same)
      : current{std::move(uit)...}
    {}

    constexpr zip_iterator(Deref deref, UIt... uit)
        requires(!_all_same)
      : deref_{std::move(deref)}, current{std::move(uit)...}
    {}

    constexpr zip_iterator(UIt... uit)
        requires(Deref::proxy && _all_same)
    {
        // tuple2array
        [&](auto... args)
        {
            size_t i = 0;
            ((current[i++] = std::move(args)), ...);
        }(std::move(uit)...);
    }

    constexpr zip_iterator(Deref deref, UIt... uit)
        requires(_all_same)
      : deref_{std::move(deref)}
    {
        // tuple2array
        [&](auto... args)
        {
            size_t i = 0;
            ((current[i++] = std::move(args)), ...);
        }(std::move(uit)...);
    }

    constexpr zip_iterator(std::array<first_uit_t, _size> const & arr)
        requires(Deref::proxy && _all_same)
      : current{std::move(arr)}
    {}

    constexpr zip_iterator(Deref deref, std::array<first_uit_t, _size> const & arr)
        requires(_all_same)
      : deref_{std::move(deref)}, current{std::move(arr)}
    {}

    template <typename... UIt2>
    constexpr zip_iterator(zip_iterator<kind, Deref, UIt2...> other)
        requires((!std::same_as<UIt2, UIt> || ...) && (std::convertible_to<UIt2, UIt> && ...) &&
                 sizeof...(UIt2) == _size && zip_iterator<kind, Deref, UIt2...>::_all_same == _all_same)
      : deref_{std::move(other.deref_)}
    {
        if constexpr (_all_same)
            std::ranges::move(other.current, current.data());
        else
            current = std::move(other.current);
    }

    constexpr decltype(auto) operator*() const { return std::apply(deref_, current); }

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
        return *(*this + n);
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
      Deref::proxy ? ((noexcept(std::ranges::iter_move(UIt{})) && ...) &&
                      (std::is_nothrow_move_constructible_v<std::iter_rvalue_reference_t<UIt>> && ...))
                   : noexcept(*i))
    {
        if constexpr (Deref::proxy)
            return tuple_transform(std::ranges::iter_move, i.current);
        /* same rule as radr::detail::transform_iterator */
        else if constexpr (std::is_lvalue_reference_v<decltype(*i)>)
            return std::move(*i);
        else
            return *i;
    }

    friend constexpr void iter_swap(zip_iterator const & lhs, zip_iterator const & rhs) noexcept(
      (noexcept(std::ranges::iter_swap(UIt{}, UIt{})) && ...))
        requires(Deref::proxy && (std::indirectly_swappable<UIt> && ...))
    {
        [&]<size_t... I>(std::index_sequence<I...>)
        {
            (std::ranges::iter_swap(std::get<I>(lhs.current), std::get<I>(rhs.current)), ...);
        }(std::make_index_sequence<sizeof...(UIt)>{});
    }
};

template <typename... UIt>
zip_iterator(UIt...) -> zip_iterator<zip_iterator_kind::adaptor, zip_deref, UIt...>;

template <zip_iterator_kind k, typename... UIt>
constexpr auto make_zip_it(UIt... uit)
{
    return zip_iterator<k, zip_deref, UIt...>{std::forward<UIt>(uit)...};
}

//!\brief radr::detail::make_zip_it with an explicit dereference policy.
template <zip_iterator_kind k, typename Deref, typename... UIt>
constexpr auto make_zip_it_with(Deref deref, UIt... uit)
{
    return zip_iterator<k, Deref, UIt...>{std::move(deref), std::forward<UIt>(uit)...};
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

    template <zip_iterator_kind _, typename Deref>
    constexpr explicit zip_sentinel(zip_iterator<_, Deref, UIt...>, std::tuple<USen...> usens) : end{std::move(usens)}
    {}

    template <typename... UIt2, typename... USen2>
    constexpr zip_sentinel(zip_sentinel<std::tuple<UIt2...>, std::tuple<USen2...>> other)
        requires(((!std::same_as<USen2, USen> || ...) || (!std::same_as<UIt2, UIt> || ...)) &&
                 (std::convertible_to<USen2, USen> && ...))
      : end{std::move(other.end)}
    {}

    template <zip_iterator_kind k, typename Deref>
    friend constexpr bool operator==(zip_iterator<k, Deref, UIt...> const & lhs, zip_sentinel const & rhs)
    {
        return [&]<size_t... I>(std::index_sequence<I...>)
        {
            return ((std::get<I>(lhs.current) == std::get<I>(rhs.end)) || ...);
        }(std::make_index_sequence<sizeof...(UIt)>{});
    }

    template <zip_iterator_kind k, typename Deref>
    friend constexpr std::iter_difference_t<zip_iterator<k, Deref, UIt...>> operator-(
      zip_iterator<k, Deref, UIt...> const & lhs,
      zip_sentinel const &                   rhs)
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

    template <zip_iterator_kind k, typename Deref>
    friend constexpr std::iter_difference_t<zip_iterator<k, Deref, UIt...>> operator-(
      zip_sentinel const &                   lhs,
      zip_iterator<k, Deref, UIt...> const & rhs)
        requires((std::sized_sentinel_for<USen, UIt> && ...))
    {
        return -(rhs - lhs);
    }
};

template <zip_iterator_kind k, typename Deref, typename... UIt, typename... USen>
zip_sentinel(zip_iterator<k, Deref, UIt...>, std::tuple<USen...>)
  -> zip_sentinel<std::tuple<UIt...>, std::tuple<USen...>>;

} // namespace radr::detail
