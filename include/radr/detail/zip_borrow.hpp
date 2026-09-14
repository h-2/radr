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

#include <ranges>
#include <tuple>
#include <utility>

#include "radr/class/borrowing_rad.hpp"
#include "radr/concepts.hpp"
#include "radr/detail/zip_iterator.hpp"
#include "radr/range_access.hpp"

namespace radr::detail
{

template <zip_iterator_kind k>
inline constexpr auto zip_with_borrow_impl = []<typename Deref, typename... URanges>(Deref deref, URanges &&... rngs)
{
    auto beg  = make_zip_it_with<k>(deref, radr::begin(rngs)...);
    auto cbeg = make_zip_it_with<k>(deref, radr::cbegin(rngs)...);

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
        auto end  = make_zip_it_with<k>(deref, radr::end(rngs)...);
        auto cend = make_zip_it_with<k>(deref, radr::cend(rngs)...);

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
