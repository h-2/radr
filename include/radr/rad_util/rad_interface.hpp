// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 The LLVM Project
// Copyright (c) 2023 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <ranges>

#include "../detail/detail.hpp"
#include "../range_access.hpp"

namespace radr
{

template <class Derived>
    requires std::is_class_v<Derived> && std::same_as<Derived, std::remove_cv_t<Derived>>
class rad_interface
{
    constexpr Derived & derived() noexcept
    {
        static_assert(sizeof(Derived) && std::derived_from<Derived, rad_interface>);
        return static_cast<Derived &>(*this);
    }

    constexpr Derived const & derived() const noexcept
    {
        static_assert(sizeof(Derived) && std::derived_from<Derived, rad_interface>);
        return static_cast<Derived const &>(*this);
    }

public:
    template <class D2 = Derived>
    [[nodiscard]] constexpr bool empty()
        requires std::ranges::forward_range<D2>
    {
        return derived().begin() == derived().end();
    }

    template <class D2 = Derived>
    [[nodiscard]] constexpr bool empty() const
        requires std::ranges::forward_range<const D2>
    {
        return derived().begin() == derived().end();
    }

    template <class D2 = Derived>
    constexpr explicit operator bool()
        requires requires(D2 & t) { t.empty(); }
    {
        return !derived().empty();
    }

    template <class D2 = Derived>
    constexpr explicit operator bool() const
        requires requires(const D2 & t) { t.empty(); }
    {
        return !derived().empty();
    }

    template <class D2 = Derived>
    constexpr auto data()
        requires std::contiguous_iterator<std::ranges::iterator_t<D2>>
    {
        // NOTE: using std::ranges::begin here explicitly because radr::begin requires type to be complete
        // for .data() this doesn't make a difference
        return std::to_address(derived().begin());
    }

    template <class D2 = Derived>
    constexpr auto data() const
        requires std::ranges::range<const D2> && std::contiguous_iterator<std::ranges::iterator_t<const D2>>
    {
        // NOTE: using std::ranges::begin here explicitly because radr::begin requires type to be complete
        // for .data() this doesn't make a difference
        return std::to_address(derived().begin());
    }

    template <class D2 = Derived>
    constexpr auto size()
        requires std::ranges::forward_range<D2> &&
                 std::sized_sentinel_for<std::ranges::sentinel_t<D2>, std::ranges::iterator_t<D2>>
    {
        return detail::to_unsigned_like(derived().end() - derived().begin());
    }

    template <class D2 = Derived>
    constexpr auto size() const
        requires std::ranges::forward_range<const D2> &&
                 std::sized_sentinel_for<std::ranges::sentinel_t<const D2>, std::ranges::iterator_t<const D2>>
    {
        return detail::to_unsigned_like(derived().end() - derived().begin());
    }

    template <class D2 = Derived>
    constexpr decltype(auto) front()
        requires std::ranges::forward_range<D2>
    {
        assert(!empty());
        return *derived().begin();
    }

    template <class D2 = Derived>
    constexpr decltype(auto) front() const
        requires std::ranges::forward_range<const D2>
    {
        assert(!derived().empty());
        return *derived().begin();
    }

    template <class D2 = Derived>
    constexpr decltype(auto) back()
        requires std::ranges::bidirectional_range<D2> && std::ranges::common_range<D2>
    {
        assert(!derived().empty());
        return *std::ranges::prev(derived().end());
    }

    template <class D2 = Derived>
    constexpr decltype(auto) back() const
        requires std::ranges::bidirectional_range<const D2> && std::ranges::common_range<const D2>
    {
        assert(!derived().empty());
        return *std::ranges::prev(derived().end());
    }

    template <std::ranges::random_access_range RARange = Derived>
    constexpr decltype(auto) operator[](std::ranges::range_difference_t<RARange> index)
    {
        return derived().begin()[index];
    }

    template <std::ranges::random_access_range RARange = Derived const>
    constexpr decltype(auto) operator[](std::ranges::range_difference_t<RARange> index) const
    {
        return derived().begin()[index];
    }

    // template <class D2 = Derived>
    // constexpr friend bool operator==(D2 const & lhs, D2 const & rhs)
    //     requires requires { { *radr::begin(lhs) == *radr::begin(rhs) } -> detail::boolean_testable; }
    // {
    //     return std::ranges::equal(lhs, rhs);
    // }

    // TODO lexicographical_compare_threeway
};

} // namespace radr
