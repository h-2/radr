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

#include "radr/class/range_interface.hpp"
#include "radr/detail/zip_iterator.hpp"
#include "radr/version.hpp"

#if !RADR_FEATURE_ZIP
#    pragma GCC warning "This header requires C++23."
#else

#    include <algorithm>
#    include <concepts>
#    include <functional>
#    include <iterator>
#    include <ranges>
#    include <type_traits>
#    include <utility>

#    include "../concepts.hpp"
#    include "radr/custom/tags.hpp"
#    include "radr/detail/detail.hpp"
#    include "radr/factory/iota.hpp"
#    include "radr/rad/reverse.hpp"
#    include "radr/range_access.hpp"

namespace radr
{

/*!\brief A container of multiple other containers.
 * \tparam URanges The underlying ranges.
 *
 * All underlying ranges must be cv-unqualified object types that model radr::mp_range.
 * At least one underlying range must be a container (i.e. not be borrowed).
 *
 * To zip over multiple borrowed ranges, use the radr::zip-factory or the radr::zip_with-adaptor. Both
 * will return a radr::borrowing_rad.
 */
template <typename... URanges>
class zip_rng : public range_interface<zip_rng<URanges...>>
{
private:
    static_assert(sizeof...(URanges) > 0, "There must be > 0 template arguments to zip_rng.");
    static_assert((range_object<URanges> && ...), "All arguments to zip_rng must be unqualified multi-passed ranges.");
    static_assert(((!borrowed_mp_range<URanges>) || ...),
                  "If all argument to zip_rng are borrowed, use radr::borrowed_rad instead.");

public:
    using iterator =
      detail::zip_iterator<detail::zip_iterator_kind::container, detail::zip_deref, iterator_t<URanges>...>;
    using const_iterator =
      detail::zip_iterator<detail::zip_iterator_kind::container, detail::zip_deref, const_iterator_t<URanges>...>;

private:
    static constexpr bool is_ra_sized =
      (safely_indexable_range<URanges const> && ...) && (std::ranges::sized_range<URanges const> || ...);
    static constexpr bool is_sized = (std::ranges::sized_range<URanges const> && ...) || is_ra_sized;
    static constexpr bool const_symmetric =
      std::same_as<std::iter_reference_t<iterator>, std::iter_reference_t<const_iterator>>;

    using size_type =
      std::conditional_t<is_sized, std::common_type_t<detail::range_size_t_or_size_t<URanges>...>, detail::not_size>;

    std::tuple<URanges...> containers;
    size_type              sz{}; // stored size

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
            constexpr auto make_zip_iterator = [](auto &&... rngs)
            {
                static constexpr get_end_t get_end{};

                // common only if end can be deduced in o(1)
                if constexpr ((common_range<URanges const> && ...) &&
                              (sizeof...(URanges) == 1 || !std::bidirectional_iterator<const_iterator>))
                {
                    return iterator_t{get_end(rngs)...};
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

    /*!\name Constructors: Rule-of-5
     * \{
     */
    constexpr zip_rng()                            = default;
    constexpr zip_rng(zip_rng const &)             = default;
    constexpr zip_rng(zip_rng &&)                  = default;
    constexpr zip_rng & operator=(zip_rng const &) = default;
    constexpr zip_rng & operator=(zip_rng &&)      = default;

    constexpr zip_rng(URanges &&... uranges) :
      containers{std::make_tuple(std::forward<URanges>(uranges)...)},
      sz{std::apply(detail::min_range_weak_size, containers)}
    {}
    //!\}

    constexpr iterator begin()
        requires(!const_symmetric)
    {
        auto make_zip_iterator = [](auto &&... rngs)
        {
            return iterator{radr::begin(rngs)...};
        };
        return std::apply(make_zip_iterator, containers);
    }

    constexpr const_iterator begin() const
    {
        auto make_zip_iterator = [](auto &&... rngs)
        {
            return const_iterator{radr::cbegin(rngs)...};
        };
        return std::apply(make_zip_iterator, containers);
    }

    constexpr auto end()
        requires(!const_symmetric)
    {
        return end_impl<iterator, decltype(radr::end)>(*this);
    }

    constexpr auto end() const { return end_impl<const_iterator, decltype(radr::cend)>(*this); }

    auto size() const
        requires is_sized
    {
        return sz;
    }
};

template <typename... URanges>
zip_rng(URanges &&...) -> zip_rng<std::remove_cvref_t<URanges>...>;

} // namespace radr

#endif
