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

#include <algorithm>
#include <concepts>
#include <functional>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

#include "../concepts.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../detail/semiregular_box.hpp"
#include "../generator.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{

template< class F, class Tuple >
constexpr auto tuple_transform( F&& f, Tuple&& tuple )
{
    return std::apply([&]<class... Ts>(Ts&&... args)
    {
        return std::tuple<std::invoke_result_t<F&, Ts>...>
            (std::invoke(f, std::forward<Ts>(args))...);
    }, std::forward<Tuple>(tuple));
}

template< class F, class Tuple1, class Tuple2 >
constexpr auto tuple_zip_transform( F&& f, Tuple1 && tuple1, Tuple2&& tuple2 )
{
    auto impl = []<size_t ... I>(F & f, Tuple1 && tuple1, Tuple2 && tuple2, std::index_sequence<I...>)
        -> std::tuple<std::invoke_result_t<F&,
                                           std::tuple_element_t<I, std::remove_cvref_t<Tuple1>>,
                                           std::tuple_element_t<I, std::remove_cvref_t<Tuple2>>>...>
    {
        return {f(std::get<I>(std::forward<Tuple1>(tuple1)), std::get<I>(std::forward<Tuple2>(tuple2)))...};
    };

    return impl(f, std::forward<Tuple1>(tuple1), std::forward<Tuple2>(tuple2), std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<Tuple1>>>{});
}


template< class F, class Tuple >
constexpr void tuple_for_each( F&& f, Tuple&& tuple )
{
    std::apply([&]<class... Ts>(Ts&&... args)
    {
        (static_cast<void>(std::invoke(f, std::forward<Ts>(args))), ...);
    }, std::forward<Tuple>(tuple));
}


template <typename ... Args>
class zip_with_sentinel;

// template <typename ... UIt, typename ... USen>
//     requires ((std::forward_iterator<UIt> && ...) && (std::sentinel_for<USen, UIt> && ...))
// class zip_with_sentinel;


template <typename ... UIt>
    requires ((std::forward_iterator<UIt> && ...))
class zip_with_iterator
{
    static_assert(sizeof...(UIt) > 1, "There must be > 1 template arguments to zip_iterator.");


    std::tuple<UIt...> current;

    template <typename ... UIt2>
        requires ((std::forward_iterator<UIt2> && ...))
    friend class zip_with_iterator;

    template <typename ... Args2>
    friend class zip_with_sentinel;

    template <typename Container>
    constexpr friend auto tag_invoke(custom::rebind_iterator_tag,
                                     zip_with_iterator it,
                                     Container &        container_old,
                                     Container &        container_new)
    {
        std::get<0>(it.current) = tag_invoke(custom::rebind_iterator_tag{}, std::get<0>(it.current), container_old, container_new);
        return it;
    }

    static constexpr bool is_random_access = (std::random_access_iterator<UIt> && ...);
    static constexpr bool is_bidi = (std::bidirectional_iterator<UIt> && ...);

public:
    // clang-format off
    using iterator_concept = std::conditional_t<is_random_access, std::random_access_iterator_tag,
                             std::conditional_t<is_bidi,          std::bidirectional_iterator_tag,
                                                                  std::forward_iterator_tag>>;
    // clang-format on

    using value_type      = std::tuple<std::iter_value_t<UIt>...>;
    using difference_type = std::common_type_t<std::iter_difference_t<UIt>...>;

    zip_with_iterator() = default;

    constexpr zip_with_iterator(UIt ... uit) :
      current{std::move(uit)...}
    {}

    template <typename ... UIt2>
    constexpr zip_with_iterator(zip_with_iterator<UIt2...> other)
        requires ((!std::same_as<UIt2, UIt> || ...) && (std::convertible_to<UIt2, UIt> && ...))
      : current{std::move(other.current)}
    {}

    // constexpr Iter const & base() const & noexcept { return current_; }
    // constexpr Iter         base() && { return std::move(current_); }
    //
    // constexpr Fn const & func() const & noexcept { return *func_; }
    // constexpr Fn         func() && { return std::move(*func_); }

    constexpr auto operator*() const
    {
        return tuple_transform([](auto & it) -> decltype(auto) { return *it;}, current);
    }

    constexpr zip_with_iterator & operator++()
    {
        tuple_for_each([](auto & it) { return ++it;}, current);
        return *this;
    }

    constexpr zip_with_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr zip_with_iterator & operator--()
        requires is_bidi
    {
        tuple_for_each([](auto & it) { return --it;}, current);
        return *this;
    }

    constexpr zip_with_iterator operator--(int)
        requires is_bidi
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr zip_with_iterator & operator+=(difference_type n)
        requires is_random_access
    {
        tuple_for_each([n] <typename It> (It & it) { return it += static_cast<std::iter_difference_t<It>>(n);}, current);
        return *this;
    }

    constexpr zip_with_iterator & operator-=(difference_type n)
        requires is_random_access
    {
        tuple_for_each([n] <typename It> (It & it) { return it -= static_cast<std::iter_difference_t<It>>(n);}, current);
        return *this;
    }

    constexpr decltype(auto) operator[](difference_type n) const
        requires is_random_access
    {
        return tuple_transform([n] <typename It> (It & it) -> decltype(auto) { return it[static_cast<std::iter_difference_t<It>>(n)];}, current);
    }

    friend constexpr bool operator==(zip_with_iterator const & lhs, zip_with_iterator const & rhs)
    {
        return lhs.current == rhs.current;
    }

    friend constexpr bool operator<(zip_with_iterator const & lhs, zip_with_iterator const & rhs)
        requires is_random_access
    {
        return lhs.current < rhs.current;
    }

    friend constexpr bool operator>(zip_with_iterator const & lhs, zip_with_iterator const & rhs)
        requires is_random_access
    {
        return lhs.current > rhs.current;
    }

    friend constexpr bool operator<=(zip_with_iterator const & lhs, zip_with_iterator const & rhs)
        requires is_random_access
    {
        return lhs.current <= rhs.current;
    }

    friend constexpr bool operator>=(zip_with_iterator const & lhs, zip_with_iterator const & rhs)
        requires is_random_access
    {
        return lhs.current >= rhs.current;
    }

    friend constexpr auto operator<=>(zip_with_iterator const & lhs, zip_with_iterator const & rhs)
        requires is_random_access && (std::three_way_comparable<UIt> && ...)
    {
        return lhs.current <=> rhs.current;
    }

    friend constexpr zip_with_iterator operator+(zip_with_iterator it, difference_type n)
        requires is_random_access
    {
        it += n;
        return it;
    }

    friend constexpr zip_with_iterator operator+(difference_type n, zip_with_iterator it)
        requires is_random_access
    {
        it += n;
        return it;
    }

    friend constexpr zip_with_iterator operator-(zip_with_iterator it, difference_type n)
        requires is_random_access
    {
        it -= n;
        return it;
    }

    friend constexpr difference_type operator-(zip_with_iterator const & lhs, zip_with_iterator const & rhs)
        requires (std::sized_sentinel_for<UIt, UIt> && ...)
    {
        return [&]<size_t ...I> (std::index_sequence<I...>)
        {
            return std::ranges::min({(std::get<I>(lhs.current) - std::get<I>(rhs.current))...});
        }(std::make_index_sequence<sizeof...(UIt)>{});
    }

    friend constexpr decltype(auto) iter_move(zip_with_iterator const & i) noexcept((noexcept(std::ranges::iter_move(UIt{})) && ...) && (std::is_nothrow_move_constructible_v<std::iter_rvalue_reference_t<UIt>> && ...) )
    {
        return tuple_transform(std::ranges::iter_move, i.current);
    }

    friend constexpr void iter_swap(zip_with_iterator const & lhs, zip_with_iterator const & rhs)
    noexcept((noexcept(std::ranges::iter_swap(UIt{}, UIt{})) && ...))
        requires ((std::indirectly_swappable<UIt> && ...))
    {
        [&]<size_t ...I> (std::index_sequence<I...>)
        {
            (std::ranges::iter_swap(std::get<I>(lhs.current), std::get<I>(rhs.current)),  ...);
        }(std::make_index_sequence<sizeof...(UIt)>{});
    }
};

template <typename ... UIt, typename ... USen>
class zip_with_sentinel<std::tuple<UIt...>, std::tuple<USen...>>
{
    static_assert(sizeof...(UIt) == sizeof...(USen), "Template arguments to zip_sentinel don't have same length.");
    static_assert(sizeof...(UIt) > 1, "There must be > 1 template arguments to zip_sentinel.");
    static_assert((std::sentinel_for<USen, UIt> && ...), "zip_sentinel's sentinel types must be sentinels for the iterator types.");

    [[no_unique_address]] std::tuple<USen...> end{};

    template <typename ... Args>
    friend class zip_with_sentinel;

public:
    zip_with_sentinel() = default;

    constexpr explicit zip_with_sentinel(USen ... usen) : end{std::move(usen)...} {}

    template <typename ... UIt2, typename ... USen2>
    constexpr zip_with_sentinel(zip_with_sentinel<std::tuple<UIt2...>, std::tuple<USen2...>> other)
        requires ((!std::same_as<USen2, USen> || ...) && (std::convertible_to<USen2, USen> && ...))
      : end{std::move(other.end)}
    {}

    // constexpr Sen base() const { return end_; }

    friend constexpr bool operator==(zip_with_iterator<UIt...> const & lhs, zip_with_sentinel const & rhs)
    {
        return [&]<size_t ...I> (std::index_sequence<I...>)
        {
            return ((std::get<I>(lhs.current) == std::get<I>(rhs.end)) || ...);
        }(std::make_index_sequence<sizeof...(UIt)>{});
    }

    friend constexpr std::iter_difference_t<zip_with_iterator<UIt...>> operator-(
      zip_with_iterator<UIt...> const & lhs,
      zip_with_sentinel const & rhs)
        requires ((std::sized_sentinel_for<USen, UIt> && ...))
    {
        return lhs.current - rhs.end;
    }

    friend constexpr std::iter_difference_t<zip_with_iterator<UIt...>> operator-(
      zip_with_sentinel const & lhs,
      zip_with_iterator<UIt...> const & rhs)
        requires ((std::sized_sentinel_for<USen, UIt> && ...))
    {
        return lhs.end - rhs.current;
    }
};



inline constexpr auto zip_with_borrow = []<typename URange, typename ... OtherRanges>(URange && urange, OtherRanges && ... others)
{
    static_assert(borrowed_mp_range<URange>, "The constraints for radr::zip_with's underlying (primary) range are not met.");

    static_assert((explicitly_borrowed_range<OtherRanges> && ...), "All ranges passed to radr::zip_with after the first need to be explicitly borrowed. Do you forget to wrap one in std::ref() or std::cref()?");

    constexpr auto impl = []<typename ... URanges>(URanges & ... rngs)
    {
        auto beg  = zip_with_iterator{radr::begin(rngs)...};
        auto cbeg = zip_with_iterator{radr::cbegin(rngs)...};

        /* all infinite → result infinite */
        if constexpr ((radr::infinite_mp_range<URanges> && ...))
        {
            return borrowing_rad{beg, cbeg, std::unreachable_sentinel, std::unreachable_sentinel};
        }
        /* all RA+sized or RA+infinite (but at least one non-infinite) → result RA+sized */
        else if constexpr ((safely_indexable_range<URanges> && ...))
        {
            using Size   = std::common_type_t<std::ranges::range_size_t<URanges>...>;
            Size const s = std::ranges::min({static_cast<Size>(capped_inf_size(rngs))...});

            auto end  = beg + s;
            auto cend = cbeg + s;

            return borrowing_rad{beg, cbeg, end, cend, s};
        }
        /* all common and at least one uni-directional → result common */
        else if constexpr ((radr::common_range<URanges> && ...) && !(std::ranges::bidirectional_range<URanges> && ...))
        {
            auto end  = zip_with_iterator{radr::end(rngs)...};
            auto cend = zip_with_iterator{radr::cend(rngs)...};

            if constexpr ((std::ranges::sized_range<URanges> && ...))
            {
                using Size = std::common_type_t<std::ranges::range_size_t<URanges>...>;
                Size const s = std::ranges::min({static_cast<Size>(std::ranges::size(rngs))...});
                return borrowing_rad{beg, cbeg, end, cend, s};
            }
            else
            {
                return borrowing_rad{beg, cbeg, end, cend, not_size{}};
            }
        }
        /* all other cases */
        else
        {
            auto end  = zip_with_sentinel{radr::end(rngs)...};
            auto cend = zip_with_sentinel{radr::cend(rngs)...};

            if constexpr ((std::ranges::sized_range<URanges> && ...))
            {
                using Size   = std::common_type_t<std::ranges::range_size_t<URanges>...>;
                Size const s = std::ranges::min({static_cast<Size>(std::ranges::size(rngs))...});
                return borrowing_rad{beg, cbeg, end, cend, s};
            }
            else
            {
                return borrowing_rad{beg, cbeg, end, cend, not_size{}};
            }
        }
    };

    return impl(urange, radr::borrow(std::forward<OtherRanges>(others))...);
};

inline constexpr auto zip_with_coro = []<std::ranges::input_range ... URanges>(URanges && ... uranges)
{

    static_assert(!(std::is_lvalue_reference_v<URanges> || ...), RADR_ASSERTSTRING_RVALUE);
    static_assert((std::movable<URanges> && ...), RADR_ASSERTSTRING_MOVABLE);

    using val_t = std::tuple<std::ranges::range_value_t<URanges>...>;
    using ref_t = std::tuple<std::ranges::range_reference_t<URanges>...>;

    return [](auto ... uranges_) -> radr::generator<ref_t, val_t>
    {
        std::tuple<iterator_t<URanges>...> its{radr::begin(uranges_)...};
        std::tuple<sentinel_t<URanges>...> const ends{radr::end(uranges_)...};

        auto at_end = [&]<size_t ...I> (std::index_sequence<I...>)
        {
            return ((std::get<I>(its) == std::get<I>(ends)) || ...);
        };

        while (!at_end(std::make_index_sequence<sizeof...(uranges)>{}))
        {
            co_yield tuple_transform([](auto & it) -> decltype(auto) { return *it;}, its);
            tuple_for_each([](auto & it) { return ++it;}, its);
        }
    }(std::move(uranges)...);
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Zips ranges with the given one.
 * \tparam URange Type of \p urange.
 * \tparam OtherRanges Types of \p other_ranges.
 * \param[in] urange The underlying range.
 * \param[in] other_ranges A pack of the other ranges; each wrapped in std::ref or std::cref.
 * \details
 *
 * |                                              |  radr::zip     |  radr::zip_with      |
 * |----------------------------------------------|----------------|----------------------|
 * | minimum arguments                            |   1            |                  1   |
 * | pipe into                                    |    no          |               yes    |
 * | lvalues of containers allowed (first range)  | std::ref-wrapped |    std::ref-wrapped |
 * | lvalues of containers allowed (other ranges) | std::ref-wrapped |    std::ref-wrapped |
 * | rvalues of containers allowed (first range)  | yes             |      yes            |
 * | rvalues of containers allowed (other ranges) | yes             |      no             |
 *
 * Use `radr::zip` when you need to capture more than one container by rvalue.
 *
 * Use `radr::zip_with` if you need to pipe-support.
 *
 * ## Multi-pass adaptor
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * Each type in `OtherRanges` needs to be std::reference_wrapper around a `radr::mp_range`.
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
 * If you want to zip more than one container, use the `radr::zip` factory instead.
 *
 * ### Notable difference to `std::views::zip`
 *
 *  * You can pipe into `radr::zip_with`, e.g. `std::string_view{"ABC} | radr::zip_with(std::string_view{"DEF"})`.
 *  * At least one range needs to be given.
 *
 * ## Single-pass adaptor
 *
 */
inline constexpr auto zip_with = detail::pipe_with_args_fn{detail::zip_with_coro, detail::zip_with_borrow};
} // namespace cpo
} // namespace radr
