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

#include <iterator>
#include <ranges>

#include "../detail/detail.hpp"
#include "../rad_util/borrowing_rad.hpp"
#include "../range_access.hpp"
#include "tags.hpp"

namespace radr::custom
{

// TODO overloads for subrange and empty_view and iota_views that are borrowed
// technically, all RA+sized borrowed ranges from std could have overloads; but likely too much work

} // namespace radr::custom

namespace radr
{

//=============================================================================
// Wrapper function subborrow
//=============================================================================

struct subborrow_impl_t
{
    /*!\name FALBACK IMPLEMENTATIONS
     * \{
     */

    /*!\brief Default implementation for creating subrange from (it,sen).
     * \param r Range the iterators originate from (just used as tag).
     * \param b Begin of the subrange.
     * \param e End of the subrange.
     * \pre (b, e) denotes a valid range.
     */
    template <const_borrowable_range URange, detail::is_iterator_of<URange> It, typename Sen>
    static constexpr auto default_(URange && /*r*/, It const b, Sen const e)
    {
        // contiguous to pointer
        if constexpr (std::contiguous_iterator<It> && std::sized_sentinel_for<Sen, It>)
        {
            using It_     = decltype(std::to_address(b));
            using ConstIt = detail::ptr_to_const_ptr_t<It_>;
            return borrowing_rad<It_, It_, ConstIt, ConstIt>{std::to_address(b), std::to_address(b) + (e - b)};
        }
        else
        {
            using ConstIt = std::conditional_t<constant_iterator<It>, It, detail::std_const_iterator_t<URange>>;
            static_assert(std::convertible_to<It, ConstIt>, RADR_BUG(__FILE__, __LINE__));
            static_assert(constant_iterator<ConstIt>,
                          "No usable constant iterator could be deduced for your range. "
                          "Building in C++23 (or later) mode might fix this.");

            // RA+sized to common
            if constexpr (std::random_access_iterator<It> && std::sized_sentinel_for<Sen, It>)
            {
                return borrowing_rad<It, It, ConstIt, ConstIt>{b, b + (e - b)};
            }
            else if constexpr (detail::is_sentinel_of<Sen, URange>)
            {
                using ConstSen =
                  std::conditional_t<std::same_as<It, Sen>, ConstIt, detail::std_const_sentinel_t<URange>>;
                static_assert(std::convertible_to<Sen, ConstSen>, RADR_BUG(__FILE__, __LINE__));
                static_assert(std::sentinel_for<ConstSen, ConstIt>, RADR_BUG(__FILE__, __LINE__));
                return borrowing_rad<It, Sen, ConstIt, ConstSen>{b, e};
            }
            else if constexpr (std::sentinel_for<Sen, It> && std::sentinel_for<Sen, ConstIt>)
            {
                return borrowing_rad<It, Sen, ConstIt, Sen>{b, e};
            }
            else
            {
                static_assert(detail::is_sentinel_of<Sen, URange> /*FALSE*/,
                              "No usable constant sentinel could be deduced for your range. "
                              "Building in C++23 (or later) mode might fix this.");
            }
        }
    }

    /*!\brief Default implementation for creating a subrange from (it,sen,size).
     * \param r Range the iterators originate from (just used as tag).
     * \param b Begin of the subrange.
     * \param e End of the subrange.
     * \param s Size of the subrange.
     * \pre (b, e) denotes a valid range.
     * \pre s is equal to std::ranges::distance(b, e)
     */
    template <const_borrowable_range URange, detail::is_iterator_of<URange> It, typename Sen>
    static constexpr auto default_(URange && /*r*/, It const b, Sen const e, size_t const s)
    {
        if constexpr (std::sized_sentinel_for<Sen, It>)
        {
            assert(int64_t(e - b) == int64_t(s));
        }

        // contiguous to pointer
        if constexpr (std::contiguous_iterator<It>)
        {
            (void)e;
            using It_     = decltype(std::to_address(b));
            using ConstIt = detail::ptr_to_const_ptr_t<It_>;
            return borrowing_rad<It_, It_, ConstIt, ConstIt>{std::to_address(b), std::to_address(b) + s};
        }
        else
        {
            using ConstIt = std::conditional_t<constant_iterator<It>, It, detail::std_const_iterator_t<URange>>;
            static_assert(std::convertible_to<It, ConstIt>, RADR_BUG(__FILE__, __LINE__));
            static_assert(constant_iterator<ConstIt>,
                          "No usable constant iterator could be deduced for your range. "
                          "Building in C++23 (or later) mode might fix this.");

            // RA+sized to common
            if constexpr (std::random_access_iterator<It>)
            {
                (void)e;
                return borrowing_rad<It, It, ConstIt, ConstIt, borrowing_rad_kind::sized>{b, b + s};
            }
            else if constexpr (detail::is_sentinel_of<Sen, URange>)
            {
                using ConstSen =
                  std::conditional_t<std::same_as<It, Sen>, ConstIt, detail::std_const_sentinel_t<URange>>;
                static_assert(std::convertible_to<Sen, ConstSen>, RADR_BUG(__FILE__, __LINE__));
                static_assert(std::sentinel_for<ConstSen, ConstIt>, RADR_BUG(__FILE__, __LINE__));
                return borrowing_rad<It, Sen, ConstIt, ConstSen, borrowing_rad_kind::sized>{b, e, s};
            }
            else if constexpr (std::sentinel_for<Sen, It> && std::sentinel_for<Sen, ConstIt>)
            {
                return borrowing_rad<It, Sen, ConstIt, Sen, borrowing_rad_kind::sized>{b, e, s};
            }
            else
            {
                static_assert(detail::is_sentinel_of<Sen, URange> /*FALSE*/,
                              "No usable constant sentinel could be deduced for your range. "
                              "Building in C++23 (or later) mode might fix this.");
            }
        }
    }

    template <const_borrowable_range URange, detail::is_iterator_of<URange> It, std::sized_sentinel_for<It> Sen>
        requires(std::contiguous_iterator<It> && std::same_as<std::iter_reference_t<It>, char const &>)
    static constexpr auto default_(URange &&, It const b, Sen const e)
    {
        return std::string_view{b, e};
    }

    template <const_borrowable_range URange, detail::is_iterator_of<URange> It, typename Sen>
        requires(std::contiguous_iterator<It> && std::same_as<std::iter_reference_t<It>, char const &>)
    static constexpr auto default_(URange &&, It const b, [[maybe_unused]] Sen const e, size_t const s)
    {
        assert(int64_t(e - b) == int64_t(s));
        return std::string_view{b, s};
    }
    //!\}

    /*!\name Actual interface
     * \{
     */

    //!\brief Call tag_invoke if possible; call default otherwise. [it, sen]
    template <const_borrowable_range URange, detail::is_iterator_of<URange> It, typename Sen>
    constexpr auto operator()(URange && urange, It const b, Sen const e) const
    {
        if constexpr (requires { tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e); })
        {
            using ret_t = decltype(tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e));
            static_assert(radr::borrowed_range_object<ret_t>,
                          "Your customisations of radr::subborrow must always return a radr::borrowed_range_object.");
            return tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e);
        }
        else
        {
            return default_(std::forward<URange>(urange), b, e);
        }
    }

    //!\brief Call tag_invoke if possible (even without size); call default otherwise. [it, sen, size]
    template <const_borrowable_range URange, detail::is_iterator_of<URange> It, typename Sen>
    constexpr auto operator()(URange && urange, It const b, Sen const e, size_t const s) const
    {
        if constexpr (requires { tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e, s); })
        {
            using ret_t = decltype(tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e, s));
            static_assert(radr::borrowed_range_object<ret_t>,
                          "Your customisations of radr::subborrow must always return a radr::borrowed_range_object.");
            return tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e, s);
        }
        else if constexpr (requires { tag_invoke(custom::subborrow_tag{}, std::forward<URange>(urange), b, e); })
        {
            return operator()(std::forward<URange>(urange), b, e);
        }
        else
        {
            return default_(std::forward<URange>(urange), b, e, s);
        }
    }

    //!\brief Call tag_invoke if possible; call default otherwise. [i, j]
    template <const_borrowable_range URange>
        requires(std::ranges::random_access_range<URange> && std::ranges::sized_range<URange>)
    constexpr auto operator()(URange && urange, size_t const start, size_t const end) const
    {
        size_t const e = std::min<size_t>(end, std::ranges::size(urange));
        size_t const b = std::min<size_t>(start, e);

        return operator()(std::forward<URange>(urange), radr::begin(urange) + b, radr::begin(urange) + e);
    }
    //!\}
};

inline constexpr subborrow_impl_t subborrow{};

template <typename... Args>
    requires(requires { subborrow(std::declval<Args>()...); })
using subborrow_t = decltype(subborrow(std::declval<Args>()...));

//=============================================================================
// borrow()
//=============================================================================

// clang-format off
inline constexpr auto borrow =
  detail::overloaded{[]<const_borrowable_range URange>(URange && urange)
{
    if constexpr (std::ranges::sized_range<URange>)
    {
        return subborrow(std::forward<URange>(urange),
                         radr::begin(urange),
                         radr::end(urange),
                         std::ranges::size(urange));
    }
    else
    {
        return subborrow(std::forward<URange>(urange), radr::begin(urange), radr::end(urange));
    }
},
    // if a range is already borrowed and copyable, just copy it (we assume O(1) copy)
    // BUT do not do this if a copy would result in a constant_range becoming a mutable range
    []<const_borrowable_range URange>(URange && urange)
        requires(radr::borrowed_range_object<std::remove_cvref_t<URange>> &&
                 std::same_as<std::ranges::range_reference_t<URange>,
                              std::ranges::range_reference_t<std::remove_cvref_t<URange>>>)
{
    // the last check is important, because we don't want to return URange for `URange const &` input
    return std::forward<URange>(urange);
}};
// clang-format on

template <typename R>
    requires(requires { borrow(std::declval<R>()); })
using borrow_t = decltype(borrow(std::declval<R>()));

//=============================================================================
// range_fwd()
//=============================================================================

//TODO nothing uses this at the moment

inline constexpr auto range_fwd = []<std::ranges::range Range>(Range && range) -> decltype(auto)
    requires const_borrowable_range<Range> || movable_range<Range>
{
    if constexpr (const_borrowable_range<Range>)
        return borrow(std::forward<Range>(range));
    else
        return std::forward<Range>(range);
};

} // namespace radr

#define RADR_FWD(R) radr::range_fwd(std::forward<decltype(R)>(R))
