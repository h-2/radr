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

#include "radr/class/borrowing_rad.hpp"
#include "radr/class/range_interface.hpp"
#include "radr/concepts.hpp"
#include "radr/custom/tags.hpp"
#include "radr/detail/detail.hpp"
#include "radr/detail/semiregular_box.hpp"
#include "radr/range_access.hpp"
#include "radr/version.hpp"

/* This is the machinery for zip-based ranges, including
 *  policies:   zip_policy_tuple, zip_policy_transform
 *  class:      zip_container
 *  factory:    zip
 *  adaptors:   zip_with, enumerate, adjacent, pairwise
 *
 * And also for the _transform variants:
 *  factory:    zip_transform
 *  adaptors:   zip_with_transform, adjacent_transform, pairwise_transform
 *
 * The machinery itself does not require C++23, but every entity built on radr::zip_policy_tuple does,
 * because it has tuple-of-reference as the range reference type. This is only supported
 * with P2321R2 (C++23).
 *
 * The *_transform variants build on radr::zip_policy_transform and are fine in C++20.
 */

namespace radr
{

/*!\brief Dereference policy of the zip machinery yielding a std::tuple of references.
 * \details
 *
 * This is the policy behind radr::zip, radr::zip_with, radr::adjacent and radr::enumerate, and it is the
 * policy of radr::zip_rng.
 *
 * The std::tuple value_type restricts instantiations to C++23.
 */
struct zip_policy_tuple
{
    //!\brief Proxy reference; selects the tuple-flavoured iter_move/iter_swap.
    static constexpr bool proxy = true;

    //!\brief The value type for a given pack of underlying iterators.
    template <typename... UIt>
    using value_type = std::tuple<std::iter_value_t<UIt>...>;

    //!\brief Returns a std::tuple of the dereferenced underlying iterators.
    template <typename... UIt>
    constexpr auto operator()(UIt const &... its) const
    {
        return std::tuple<std::iter_reference_t<UIt>...>(*its...);
    }
};

/*!\brief Dereference policy of the zip machinery invoking an N-ary functor.
 * \tparam Fn The invocable; it is called with N arguments, not with a tuple.
 * \details
 *
 * This is the policy behind the `*_transform` adaptors and behind radr::zip_transform.
 *
 * No std::tuple appears in any associated type, so instantiations are C++20.
 */
template <typename Fn>
struct zip_policy_transform
{
    //!\brief No proxy; iter_move follows radr::detail::transform_iterator and iter_swap is dropped.
    static constexpr bool proxy = false;

    //!\brief The stored invocable.
    [[no_unique_address]] detail::semiregular_box<Fn> fn{};

    //!\brief The value type for a given pack of underlying iterators.
    template <typename... UIt>
    using value_type = std::remove_cvref_t<std::invoke_result_t<Fn const &, std::iter_reference_t<UIt>...>>;

    //!\brief Returns the result of invoking the functor on the dereferenced underlying iterators.
    template <typename... UIt>
    constexpr decltype(auto) operator()(UIt const &... its) const
    {
        return std::invoke(*fn, *its...);
    }
};

//!\brief Requirements of radr::zip_policy_transform on \p Fn, for one pack of underlying iterators.
template <typename Fn, typename... UIts>
concept zip_policy_transform_constraints =
  std::is_object_v<Fn> && std::regular_invocable<Fn const &, std::iter_reference_t<UIts>...> &&
  detail::can_reference<std::invoke_result_t<Fn const &, std::iter_reference_t<UIts>...>>;

} // namespace radr

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

template <zip_iterator_kind kind, typename ZipPolicy, typename... UIt>
    requires((std::forward_iterator<UIt> && ...))
class zip_iterator
{
    static constexpr size_t _size = sizeof...(UIt);
    static_assert(_size > 0, "There must be > 0 template arguments to zip_iterator.");

    using first_uit_t               = pack_head_t<UIt...>;
    static constexpr bool _all_same = (std::same_as<UIt, first_uit_t> && ...);

    static_assert((kind != zip_iterator_kind::adjacent) || _all_same,
                  "Adjacent means all iterator types are the same.");

    template <zip_iterator_kind kind2, typename ZipPolicy2, typename... UIt2>
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

    [[no_unique_address]] ZipPolicy policy_{};
    storage_type                    current;

    /*!\brief Move-assign \p rhs onto \p lhs, elementwise if the two storages differ.
     * \details The element-wise path covers std::tuple <-> std::array.
     */
    template <typename Lhs, typename Rhs>
    static constexpr void move_assign_elements(Lhs && lhs, Rhs && rhs)
    {
        if constexpr (decays_to<Lhs, Rhs>)
        {
            lhs = std::move(rhs);
        }
        else
        {
            [&]<size_t... I>(std::index_sequence<I...>)
            {
                ((std::get<I>(lhs) = std::move(std::get<I>(rhs))), ...);
            }(std::make_index_sequence<_size>{});
        }
    }

public:
    // clang-format off
    using iterator_concept = std::conditional_t<is_random_access, std::random_access_iterator_tag,
                             std::conditional_t<is_bidi,          std::bidirectional_iterator_tag,
                                                                  std::forward_iterator_tag>>;
    // clang-format on

    using value_type      = typename ZipPolicy::template value_type<UIt...>;
    using difference_type = std::common_type_t<std::iter_difference_t<UIt>...>;

    /*!\brief Access to the underlying iterators.
     * \details The sentinels go through this instead of touching `current` directly, although they are friends:
     *          GCC<=12 does not grant a befriended class's friendship to functions *defined inside* that class,
     *          which is what every sentinel's `operator==` / `operator-` is. Same workaround as in
     *          radr::detail::transform_sentinel.
     *
     *          Deliberately *not* called `base()`: the generic fallback in `custom/rebind_iterator.hpp`
     *          is keyed on `it.base()` plus `It(it.base())`, and a `base()` here makes that fallback a
     *          viable candidate next to this iterator's own `tag_invoke` for the zip_policy_tuple flavour.
     */
    constexpr storage_type const & base_storage() const & noexcept { return current; }

    zip_iterator() = default;

    /* Policy-less constructors default-construct policy_; restricted to zip_policy_tuple, because
     * zip_policy_transform<Fn> would hold an empty semiregular_box for non-default-constructible Fn. */
    constexpr zip_iterator(UIt... uit)
        requires(ZipPolicy::proxy)
    {
        move_assign_elements(current, std::tuple(std::move(uit)...));
    }

    constexpr zip_iterator(ZipPolicy policy, UIt... uit) : policy_{std::move(policy)}
    {
        move_assign_elements(current, std::tuple(std::move(uit)...));
    }

    constexpr zip_iterator(storage_type rhs)
        requires(ZipPolicy::proxy)
    {
        move_assign_elements(current, rhs);
    }

    constexpr zip_iterator(ZipPolicy policy, storage_type rhs) : policy_{std::move(policy)}
    {
        move_assign_elements(current, rhs);
    }

    template <typename... UIt2>
    constexpr zip_iterator(zip_iterator<kind, ZipPolicy, UIt2...> other)
        requires((!std::same_as<UIt2, UIt> || ...) && (std::convertible_to<UIt2, UIt> && ...) &&
                 sizeof...(UIt2) == _size)
      : policy_{std::move(other.policy_)}
    {
        move_assign_elements(current, other.current);
    }

    constexpr decltype(auto) operator*() const { return std::apply(policy_, current); }

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
      ZipPolicy::proxy ? ((noexcept(std::ranges::iter_move(UIt{})) && ...) &&
                          (std::is_nothrow_move_constructible_v<std::iter_rvalue_reference_t<UIt>> && ...))
                       : noexcept(*i))
    {
        if constexpr (ZipPolicy::proxy)
            return tuple_transform(std::ranges::iter_move, i.current);
        /* same rule as radr::detail::transform_iterator */
        else if constexpr (std::is_lvalue_reference_v<decltype(*i)>)
            return std::move(*i);
        else
            return *i;
    }

    friend constexpr void iter_swap(zip_iterator const & lhs, zip_iterator const & rhs) noexcept(
      (noexcept(std::ranges::iter_swap(UIt{}, UIt{})) && ...))
        requires(ZipPolicy::proxy && (std::indirectly_swappable<UIt> && ...))
    {
        [&]<size_t... I>(std::index_sequence<I...>)
        {
            (std::ranges::iter_swap(std::get<I>(lhs.current), std::get<I>(rhs.current)), ...);
        }(std::make_index_sequence<sizeof...(UIt)>{});
    }
};

template <typename... UIt>
zip_iterator(UIt...) -> zip_iterator<zip_iterator_kind::adaptor, zip_policy_tuple, UIt...>;

template <zip_iterator_kind k, typename... UIt>
constexpr auto make_zip_it(UIt... uit)
{
    return zip_iterator<k, zip_policy_tuple, UIt...>{std::forward<UIt>(uit)...};
}

//!\brief radr::detail::make_zip_it with an explicit dereference policy.
template <zip_iterator_kind k, typename ZipPolicy, typename... UIt>
constexpr auto make_zip_it_with(ZipPolicy policy, UIt... uit)
{
    return zip_iterator<k, ZipPolicy, UIt...>{std::move(policy), std::forward<UIt>(uit)...};
}

/*!\brief Sentinel type for zip_iterator_kind::adaptor and zip_iterator_kind::container.
 * \details Used by the zip_with adaptor and by radr::zip_container, in both cases only when the end cannot be
 * deduced in o(1). enumerate and adjacent have their own sentinels.
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

    /* Unlike zip_iterator, this is also injected for the container; harmless, because radr::zip_container is
     * always at the "source" of a pipe and is therefore never rebound. */
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

    template <zip_iterator_kind _, typename ZipPolicy>
    constexpr explicit zip_sentinel(zip_iterator<_, ZipPolicy, UIt...>, std::tuple<USen...> usens) :
      end{std::move(usens)}
    {}

    template <typename... UIt2, typename... USen2>
    constexpr zip_sentinel(zip_sentinel<std::tuple<UIt2...>, std::tuple<USen2...>> other)
        requires(((!std::same_as<USen2, USen> || ...) || (!std::same_as<UIt2, UIt> || ...)) &&
                 (std::convertible_to<USen2, USen> && ...))
      : end{std::move(other.end)}
    {}

    template <zip_iterator_kind k, typename ZipPolicy>
    friend constexpr bool operator==(zip_iterator<k, ZipPolicy, UIt...> const & lhs, zip_sentinel const & rhs)
    {
        return [&]<size_t... I>(std::index_sequence<I...>)
        {
            return ((std::get<I>(lhs.base_storage()) == std::get<I>(rhs.end)) || ...);
        }(std::make_index_sequence<sizeof...(UIt)>{});
    }

    template <zip_iterator_kind k, typename ZipPolicy>
    friend constexpr std::iter_difference_t<zip_iterator<k, ZipPolicy, UIt...>> operator-(
      zip_iterator<k, ZipPolicy, UIt...> const & lhs,
      zip_sentinel const &                       rhs)
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
        return std::apply(pack_min, detail::tuple_zip_transform(diff, lhs.base_storage(), rhs.end));
    }

    template <zip_iterator_kind k, typename ZipPolicy>
    friend constexpr std::iter_difference_t<zip_iterator<k, ZipPolicy, UIt...>> operator-(
      zip_sentinel const &                       lhs,
      zip_iterator<k, ZipPolicy, UIt...> const & rhs)
        requires((std::sized_sentinel_for<USen, UIt> && ...))
    {
        return -(rhs - lhs);
    }
};

template <zip_iterator_kind k, typename ZipPolicy, typename... UIt, typename... USen>
zip_sentinel(zip_iterator<k, ZipPolicy, UIt...>, std::tuple<USen...>)
  -> zip_sentinel<std::tuple<UIt...>, std::tuple<USen...>>;

template <zip_iterator_kind k>
inline constexpr auto zip_with_borrow_impl =
  []<typename ZipPolicy, typename... URanges>(ZipPolicy policy, URanges &&... rngs)
{
    auto beg  = make_zip_it_with<k>(policy, radr::begin(rngs)...);
    auto cbeg = make_zip_it_with<k>(policy, radr::cbegin(rngs)...);

    auto const min_size = min_range_size(rngs...);

    /* all infinite → result infinite */
    if constexpr ((infinite_mp_range<URanges> && ...))
    {
        return borrowing_rad{beg, std::unreachable_sentinel, cbeg, std::unreachable_sentinel};
    }
    /* all RA+sized or RA+infinite (but at least one non-infinite) → result RA+sized */
    else if constexpr ((safely_indexable_range<URanges> && ...))
    {
        auto end  = beg + min_size;
        auto cend = cbeg + min_size;

        return borrowing_rad{beg, end, cbeg, cend, min_size};
    }
    /* we only preserve common for 1-dimensional or uni-directional (because then unsynced ends are irrelevant) */
    else if constexpr ((common_range<URanges> && ...) &&
                       (sizeof...(URanges) == 1 || !(std::ranges::bidirectional_range<URanges> && ...)))
    {
        auto end  = make_zip_it_with<k>(policy, radr::end(rngs)...);
        auto cend = make_zip_it_with<k>(policy, radr::cend(rngs)...);

        return borrowing_rad{beg, end, cbeg, cend, min_size};
    }
    /* all other cases */
    else
    {
        auto end  = zip_sentinel{beg, std::make_tuple(radr::end(rngs)...)};
        auto cend = zip_sentinel{cbeg, std::make_tuple(radr::cend(rngs)...)};

        return borrowing_rad{beg, end, cbeg, cend, min_size};
    }
};

} // namespace radr::detail

namespace radr
{

/*!\brief A container of multiple other containers.
 * \tparam ZipPolicy The dereference policy; radr::zip_policy_tuple or radr::zip_policy_transform.
 * \tparam URanges The underlying ranges.
 * \details
 *
 * All underlying ranges must be cv-unqualified object types that model radr::mp_range.
 * At least one underlying range must be a container (i.e. not be borrowed).
 *
 * It is recommended to use one of the following adaptors or factories instead of using this type directly:
 *   * radr::zip
 *   * radr::zip_with
 *   * radr::zip_transform
 *   * radr::zip_with_transform
 *
 * Note that the simple "zip" mechanism (enabled via radr::zip_policy_tuple) is only available in C++23.
 */
template <typename ZipPolicy, typename... URanges>
class zip_container : public range_interface<zip_container<ZipPolicy, URanges...>>
{
private:
#if RADR_FEATURE_ZIP
    static constexpr bool radr_has_feature_zip = true;
#else
    static constexpr bool radr_has_feature_zip = false;
#endif

    static_assert(radr_has_feature_zip || detail::different_from<ZipPolicy, zip_policy_tuple>,
                  "Ranges whose elements are tuples of references are only available in C++23.");

    static_assert(sizeof...(URanges) > 0, "There must be > 0 range arguments to zip_container.");
    static_assert((range_object<URanges> && ...),
                  "All range arguments to zip_container must be unqualified multi-passed ranges.");
    static_assert(((!borrowed_mp_range<URanges>) || ...),
                  "If all range arguments to zip_container are borrowed, use radr::borrowed_rad instead.");

    using iterator = detail::zip_iterator<detail::zip_iterator_kind::container, ZipPolicy, iterator_t<URanges>...>;
    using const_iterator =
      detail::zip_iterator<detail::zip_iterator_kind::container, ZipPolicy, const_iterator_t<URanges>...>;

    static constexpr bool is_ra_sized =
      (safely_indexable_range<URanges const> && ...) && (std::ranges::sized_range<URanges const> || ...);
    static constexpr bool is_sized        = (std::ranges::sized_range<URanges const> && ...) || is_ra_sized;
    static constexpr bool const_symmetric = std::same_as<iterator, const_iterator>;

    using size_type =
      std::conditional_t<is_sized, std::common_type_t<detail::range_size_t_or_size_t<URanges>...>, detail::not_size>;

    [[no_unique_address]] ZipPolicy policy{};
    std::tuple<URanges...>          containers;
    size_type                       sz{}; // stored size

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
            auto make_zip_iterator = [&](auto &&... rngs)
            {
                static constexpr get_end_t get_end{};

                // common only if end can be deduced in o(1)
                if constexpr ((common_range<URanges const> && ...) &&
                              (sizeof...(URanges) == 1 || !std::bidirectional_iterator<const_iterator>))
                {
                    return iterator_t{self.policy, get_end(rngs)...};
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

    /*!\name Constructors, destructors and assignment
     * \{
     */
    constexpr zip_container()                                  = default;
    constexpr zip_container(zip_container const &)             = default;
    constexpr zip_container(zip_container &&)                  = default;
    constexpr zip_container & operator=(zip_container const &) = default;
    constexpr zip_container & operator=(zip_container &&)      = default;

    constexpr zip_container(ZipPolicy policy_, URanges &&... uranges) :
      policy{std::move(policy_)},
      containers{std::make_tuple(std::forward<URanges>(uranges)...)},
      sz{std::apply(detail::min_range_weak_size, containers)}
    {}

    constexpr zip_container(URanges &&... uranges)
        requires std::same_as<ZipPolicy, zip_policy_tuple>
      :
      policy{zip_policy_tuple{}},
      containers{std::make_tuple(std::forward<URanges>(uranges)...)},
      sz{std::apply(detail::min_range_weak_size, containers)}
    {}
    //!\}

    constexpr iterator begin()
        requires(!const_symmetric)
    {
        auto make_zip_iterator = [this](auto &&... rngs)
        {
            return iterator{policy, radr::begin(rngs)...};
        };
        return std::apply(make_zip_iterator, containers);
    }

    constexpr const_iterator begin() const
    {
        auto make_zip_iterator = [this](auto &&... rngs)
        {
            return const_iterator{policy, radr::cbegin(rngs)...};
        };
        return std::apply(make_zip_iterator, containers);
    }

    constexpr auto end()
        requires(!const_symmetric)
    {
        return end_impl<iterator, decltype(radr::end)>(*this);
    }

    constexpr auto end() const { return end_impl<const_iterator, decltype(radr::cend)>(*this); }

    constexpr auto size() const
        requires is_sized
    {
        return sz;
    }

    constexpr friend bool operator==(zip_container const & lhs, zip_container const & rhs)
        requires detail::weakly_equality_comparable<std::iter_reference_t<const_iterator>>
    {
        return std::ranges::equal(lhs, rhs);
    }
};

template <typename ZipPolicy, typename... URanges>
zip_container(ZipPolicy, URanges &&...) -> zip_container<ZipPolicy, std::remove_cvref_t<URanges>...>;

template <typename... URanges>
    requires(mp_range<URanges> && ...)
zip_container(URanges &&...) -> zip_container<zip_policy_tuple, std::remove_cvref_t<URanges>...>;

} // namespace radr
