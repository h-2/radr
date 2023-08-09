// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <functional>
#include <ranges>

#include "cached_bounds.hpp"
#include "concepts.hpp"
#include "detail.hpp"
#include "generator.hpp"
#include "rad_interface.hpp"
#include "range_ref.hpp"

namespace radr
{

inline constexpr auto take_coro = []<adaptable_range URange>(URange && urange, size_t const n)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);

    // we need to create inner functor so that it can take by value
    return
      [](auto         urange_,
         size_t const n) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        size_t i = 0;
        for (auto it = std::ranges::begin(urange_); it != std::ranges::end(urange_) && i < n; ++it, ++i)
            co_yield *it;
    }(std::move(urange), n);
};

} // namespace radr

namespace radr::detail
{

struct take_fn
{
    template <std::ranges::input_range Range>
    [[nodiscard]] constexpr auto operator()(Range && range, size_t const n) const
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_RVALUE_ASSERTION_STRING);
        return take_coro(std::forward<Range>(range), n);
    }

    template <std::ranges::forward_range Range>
    [[nodiscard]] constexpr auto operator()(Range && range, size_t const n) const
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_RVALUE_ASSERTION_STRING);

        auto fn = [n](auto & urange_)
        {
            static constexpr range_bounds_kind kind = range_bounds_kind::sized;
            // std::ranges::sized_range<Range> ? range_bounds_kind::sized : range_bounds_kind::unsized;

            if constexpr (std::ranges::random_access_range<Range> && std::ranges::sized_range<Range>)
            {
                using RangeBounds = range_bounds<std::ranges::iterator_t<Range>,
                                                 std::ranges::iterator_t<Range>,
                                                 detail::const_it_or_nullptr_t<Range>,
                                                 detail::const_it_or_nullptr_t<Range>,
                                                 kind>;

                return RangeBounds{std::ranges::begin(urange_),
                                   std::ranges::begin(urange_) + std::min<size_t>(n, std::ranges::size(urange_)),
                                   std::min<size_t>(n, std::ranges::size(urange_))};
            }
            //TODO we currently implement "take_exactly" on forward_ranges and not take (safely)
            else if constexpr (std::ranges::forward_range<Range const>)
            {
                using RangeBounds = range_bounds<std::counted_iterator<std::ranges::iterator_t<Range>>,
                                                 std::default_sentinel_t,
                                                 std::counted_iterator<std::ranges::iterator_t<Range const>>,
                                                 std::default_sentinel_t,
                                                 kind>;

                return RangeBounds{
                  std::counted_iterator<std::ranges::iterator_t<Range>>(std::ranges::begin(urange_), n),
                  std::default_sentinel,
                  n};
            }
            else
            {
                using RangeBounds = range_bounds<std::counted_iterator<std::ranges::iterator_t<Range>>,
                                                 std::default_sentinel_t,
                                                 std::nullptr_t,
                                                 std::nullptr_t,
                                                 kind>;

                return RangeBounds{
                  std::counted_iterator<std::ranges::iterator_t<Range>>(std::ranges::begin(urange_), n),
                  std::default_sentinel,
                  n};
            }
        };

        if constexpr (std::ranges::borrowed_range<Range>)
            return fn(range);
        else
            return cached_bounds_rad{std::move(range), fn};
    }

    template <class Range>
    [[nodiscard]] constexpr auto operator()(std::reference_wrapper<Range> const & range, size_t const n) const
      -> decltype(operator()(range_ref{range}, n))
    {
        return operator()(range_ref{range}, n);
    }

    [[nodiscard]] constexpr auto operator()(size_t const n) const
    {
        return range_adaptor_closure_t{detail::bind_back(*this, n)};
    }
};

} // namespace radr::detail

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto take = detail::take_fn{};
} // namespace cpo
} // namespace radr::pipe
