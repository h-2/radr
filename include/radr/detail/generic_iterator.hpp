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

#include <concepts>
#include <iterator>
#include <type_traits>
#include <utility>

#include "../class/borrowing_rad.hpp"
#include "../concepts.hpp"
#include "../custom/tags.hpp"
#include "../range_access.hpp"
#include "detail.hpp"

namespace radr::detail
{

/*!\brief CRTP base of iterators that adapt a single underlying iterator.
 * \tparam D   The derived iterator.
 * \tparam UIt The underlying iterator.
 * \details
 *
 * Provides navigation, comparison, rebind and the comparisons with std::default_sentinel_t; all operate on the
 * underlying iterator.
 *
 * The derived iterator must provide `operator*` and `value_type`. It may hide `operator[]` (default: `*(d + n)`) and
 * may add hidden-friend `iter_move`/`iter_swap` (default: the fallbacks of std::ranges). A hidden friend with the same
 * signature in both classes would be a redefinition; hence the base declares neither.
 *
 * A derived iterator that provides `fold(fn2)` is folded into by radr::transform (see transform.hpp).
 */
template <typename D, std::forward_iterator UIt>
class generic_iterator_base
{
    [[no_unique_address]] UIt uit{};

    constexpr D &       derived() noexcept { return static_cast<D &>(*this); }
    constexpr D const & derived() const noexcept { return static_cast<D const &>(*this); }

    template <typename Container>
    constexpr friend D tag_invoke(custom::rebind_iterator_tag,
                                  D           it,
                                  Container & container_old,
                                  Container & container_new)
    {
        auto & u = static_cast<generic_iterator_base &>(it).uit;
        u        = tag_invoke(custom::rebind_iterator_tag{}, u, container_old, container_new);
        return it;
    }

protected:
    generic_iterator_base() = default;

    constexpr explicit generic_iterator_base(UIt u) : uit{std::move(u)} {}

public:
    using iterator_concept =
      std::conditional_t<std::contiguous_iterator<UIt>, std::random_access_iterator_tag, iterator_tag_t<UIt>>;
    using difference_type = std::iter_difference_t<UIt>;

    constexpr UIt const & base() const & noexcept { return uit; }
    constexpr UIt         base() && { return std::move(uit); }

    constexpr decltype(auto) operator[](difference_type n) const
        requires std::random_access_iterator<UIt>
    {
        return *(derived() + n);
    }

    constexpr D & operator++()
    {
        ++uit;
        return derived();
    }

    constexpr D operator++(int)
    {
        D tmp = derived();
        ++*this;
        return tmp;
    }

    constexpr D & operator--()
        requires std::bidirectional_iterator<UIt>
    {
        --uit;
        return derived();
    }

    constexpr D operator--(int)
        requires std::bidirectional_iterator<UIt>
    {
        D tmp = derived();
        --*this;
        return tmp;
    }

    constexpr D & operator+=(difference_type n)
        requires std::random_access_iterator<UIt>
    {
        uit += n;
        return derived();
    }

    constexpr D & operator-=(difference_type n)
        requires std::random_access_iterator<UIt>
    {
        uit -= n;
        return derived();
    }

    friend constexpr bool operator==(D const & x, D const & y) { return x.base() == y.base(); }

    friend constexpr bool operator<(D const & x, D const & y)
        requires std::random_access_iterator<UIt>
    {
        return x.base() < y.base();
    }

    friend constexpr bool operator>(D const & x, D const & y)
        requires std::random_access_iterator<UIt>
    {
        return x.base() > y.base();
    }

    friend constexpr bool operator<=(D const & x, D const & y)
        requires std::random_access_iterator<UIt>
    {
        return x.base() <= y.base();
    }

    friend constexpr bool operator>=(D const & x, D const & y)
        requires std::random_access_iterator<UIt>
    {
        return x.base() >= y.base();
    }

    friend constexpr auto operator<=>(D const & x, D const & y)
        requires std::random_access_iterator<UIt> && std::three_way_comparable<UIt>
    {
        return x.base() <=> y.base();
    }

    friend constexpr D operator+(D i, difference_type n)
        requires std::random_access_iterator<UIt>
    {
        return i += n;
    }

    friend constexpr D operator+(difference_type n, D i)
        requires std::random_access_iterator<UIt>
    {
        return i += n;
    }

    friend constexpr D operator-(D i, difference_type n)
        requires std::random_access_iterator<UIt>
    {
        return i -= n;
    }

    friend constexpr difference_type operator-(D const & x, D const & y)
        requires std::sized_sentinel_for<UIt, UIt>
    {
        return x.base() - y.base();
    }

    /* std::default_sentinel_t is preserved as sentinel (instead of being wrapped in generic_sentinel) */
    friend constexpr bool operator==(D const & x, std::default_sentinel_t)
        requires std::sentinel_for<std::default_sentinel_t, UIt>
    {
        return x.base() == std::default_sentinel;
    }

    friend constexpr difference_type operator-(D const & x, std::default_sentinel_t)
        requires std::sized_sentinel_for<std::default_sentinel_t, UIt>
    {
        return x.base() - std::default_sentinel;
    }

    friend constexpr difference_type operator-(std::default_sentinel_t, D const & y)
        requires std::sized_sentinel_for<std::default_sentinel_t, UIt>
    {
        return std::default_sentinel - y.base();
    }
};

//!\brief Sentinel for every radr::detail::generic_iterator_base; independent of the derived iterator.
template <typename USen>
class generic_sentinel
{
    [[no_unique_address]] USen end_{};

    template <typename USen2>
    friend class generic_sentinel;

public:
    generic_sentinel() = default;

    constexpr explicit generic_sentinel(USen end) : end_{std::move(end)} {}

    template <different_from<USen> USen2>
        requires std::convertible_to<USen2, USen>
    constexpr generic_sentinel(generic_sentinel<USen2> s) : end_{std::move(s.end_)}
    {}

    constexpr USen base() const { return end_; }

    /* deduced against the base class: matches every derived iterator */
    template <typename D, typename UIt>
        requires std::sentinel_for<USen, UIt>
    friend constexpr bool operator==(generic_iterator_base<D, UIt> const & x, generic_sentinel const & y)
    {
        return x.base() == y.end_;
    }

    template <typename D, typename UIt>
        requires std::sized_sentinel_for<USen, UIt>
    friend constexpr std::iter_difference_t<UIt> operator-(generic_iterator_base<D, UIt> const & x,
                                                           generic_sentinel const &              y)
    {
        return x.base() - y.end_;
    }

    template <typename D, typename UIt>
        requires std::sized_sentinel_for<USen, UIt>
    friend constexpr std::iter_difference_t<UIt> operator-(generic_sentinel const &              x,
                                                           generic_iterator_base<D, UIt> const & y)
    {
        return x.end_ - y.base();
    }
};

template <typename T>
inline constexpr bool is_generic_sentinel = false;

template <typename USen>
inline constexpr bool is_generic_sentinel<generic_sentinel<USen>> = true;

//!\brief Wraps an underlying iterator-sentinel pair; \p make_it creates the derived iterator. Stateless sentinels are preserved.
inline constexpr auto generic_wrap = []<typename UIt, typename USen>(UIt uit, USen usen, auto const & make_it)
{
    if constexpr (std::same_as<UIt, USen>)
        return std::pair{make_it(std::move(uit)), make_it(std::move(usen))};
    else if constexpr (one_of<USen, std::unreachable_sentinel_t, std::default_sentinel_t>)
        return std::pair{make_it(std::move(uit)), usen};
    else
        return std::pair{make_it(std::move(uit)), generic_sentinel<USen>{std::move(usen)}};
};

//!\brief Borrows \p urange with every iterator wrapped by \p make_it.
inline constexpr auto generic_borrow = []<typename URange>(URange && urange, auto const & make_it)
{
    auto [beg, end]   = generic_wrap(radr::begin(urange), radr::end(urange), make_it);
    auto [cbeg, cend] = generic_wrap(radr::cbegin(urange), radr::cend(urange), make_it);

    return borrowing_rad{beg, end, cbeg, cend, detail::size_or_not(urange)};
};

} // namespace radr::detail
