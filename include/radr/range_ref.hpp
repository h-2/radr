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
#include <utility>

#include "detail.hpp"
#include "rad_interface.hpp"

namespace radr
{
template <std::ranges::range Range>
    requires std::is_object_v<Range>
class range_ref : public rad_interface<range_ref<Range>>
{
    Range * urange;

    static void fun(Range &);
    static void fun(Range &&) = delete;

public:
    template <class Tp>
        requires detail::different_from<Tp, range_ref> && std::convertible_to<Tp, Range &> &&
                 requires { fun(std::declval<Tp>()); }
    constexpr range_ref(Tp && t) : urange(std::addressof(static_cast<Range &>(std::forward<Tp>(t))))
    {}

    constexpr Range &       base() { return *urange; }
    constexpr Range const & base() const
        requires std::ranges::range<Range const>
    {
        return *urange;
    }

    constexpr std::ranges::iterator_t<Range> begin()
        requires(!detail::simple_range<Range>)
    {
        return std::ranges::begin(*urange);
    }

    constexpr auto begin() const
        requires std::ranges::range<Range const>
    {
        return std::ranges::begin(std::as_const(*urange));
    }

    constexpr std::ranges::sentinel_t<Range> end()
        requires(!detail::simple_range<Range>)
    {
        return std::ranges::end(*urange);
    }

    constexpr auto end() const
        requires std::ranges::range<Range const>
    {
        return std::ranges::end(std::as_const(*urange));
    }

    // constexpr bool empty() const
    //     requires requires { std::ranges::empty(*urange); }
    // {
    //     return std::ranges::empty(*urange);
    // }

    constexpr auto size()
        requires std::ranges::sized_range<Range>
    {
        return std::ranges::size(*urange);
    }

    constexpr auto size() const
        requires std::ranges::sized_range<Range const>
    {
        return std::ranges::size(std::as_const(*urange));
    }

    constexpr auto data()
        requires std::ranges::contiguous_range<Range>
    {
        return std::ranges::data(*urange);
    }

    constexpr auto data() const
        requires std::ranges::contiguous_range<Range const>
    {
        return std::ranges::data(std::as_const(*urange));
    }
};

template <class Range>
range_ref(Range &) -> range_ref<Range>;

template <class Range>
range_ref(std::reference_wrapper<Range> &) -> range_ref<Range>;

template <class Range>
range_ref(std::reference_wrapper<Range> &&) -> range_ref<Range>;

template <class Range>
range_ref(std::reference_wrapper<Range> const &) -> range_ref<Range>;

template <class Range>
range_ref(std::reference_wrapper<Range> const &&) -> range_ref<Range>;

template <std::ranges::range Range>
decltype(auto) range_fwd(Range && range)
{
    if constexpr (std::is_lvalue_reference_v<Range>)
        return range_ref{range};
    else
        return std::forward<Range>(range);
}

} // namespace radr

template <class Tp>
inline constexpr bool std::ranges::enable_view<radr::range_ref<Tp>> = true;

template <class Tp>
inline constexpr bool std::ranges::enable_borrowed_range<radr::range_ref<Tp>> = true;
