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

#include <iterator>

#include "tags.hpp"

namespace radr
{

//=============================================================================
// Wrapper function find_common_end
//=============================================================================

struct find_common_end_impl_t
{
    //!\brief Call tag_invoke if possible; call default otherwise. [it, sen]
    template <typename It, typename Sen>
    constexpr It operator()(It b, Sen e) const
    {
        static_assert(std::forward_iterator<It>,
                      "You must pass a forward_iterator as first argument to radr::find_common_end.");
        static_assert(std::sentinel_for<Sen, It>,
                      "You must pass a sentinel as second argument to radr::find_common_end.");
        static_assert(!std::same_as<Sen, std::unreachable_sentinel_t>,
                      "You must not pass infinite ranges to radr::find_common_end.");

        if constexpr (std::same_as<It, Sen>)
        {
            return e;
        }
        else if constexpr (requires { tag_invoke(custom::find_common_end_tag{}, std::move(b), std::move(e)); })
        {
            using ret_t = decltype(tag_invoke(custom::find_common_end_tag{}, std::move(b), std::move(e)));
            static_assert(std::same_as<ret_t, It>,
                          "Your customisations of radr::find_common_end must always return the iterator type.");
            return tag_invoke(custom::find_common_end_tag{}, std::move(b), std::move(e));
        }
        else
        {
            auto ret = b;
            std::ranges::advance(ret, e);
            return ret;
        }
    }

    //!\}
};

/*!\brief Given an iterator-sentinel pair, return an iterator that can be used as the sentinel.
 * \param[in] b The iterator.
 * \param[in] e The sentinel.
 * \returns An iterator that can be used with \p in to represent the same range as `(b, e)` but where iterator and sentinel have the same type.
 * \pre `(b, e)` denotes a valid, finite range.
 * \details
 *
 * NOOP if \p b and \p e already have the same type.
 *
 * ### Complexity
 *
 * The default implementation is **linear**; the end is searched from the beginning.
 *
 * ### Customisation
 *
 * You may provide overloads with the following signature to customise the behaviour:
 *
 * ```cpp
 * It tag_invoke(radr::find_common_end_tag, It, Sen);
 * ```
 *
 * They must be visible to ADL.
 *
 */
inline constexpr find_common_end_impl_t find_common_end{};

} // namespace radr
