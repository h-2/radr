// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2025 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <iterator>
#include <utility>

#include "../detail/pipe.hpp"
#include "../generator.hpp"
#include "../rad/filter.hpp"
#include "../rad_util/borrowing_rad.hpp"

namespace radr::detail
{

template <std::forward_iterator Iter, std::sentinel_for<Iter> Sen, filter_func_constraints<Iter> Func>
class take_while_sentinel
{
    [[no_unique_address]] Sen                   end_{};
    [[no_unique_address]] semiregular_box<Func> func_;

    template <std::forward_iterator Iter_, std::sentinel_for<Iter_> Sen_, filter_func_constraints<Iter_> Func_>
    friend class take_while_sentinel;

    template <typename Container>
    constexpr friend take_while_sentinel tag_invoke(custom::rebind_iterator_tag,
                                                    take_while_sentinel it,
                                                    Container &         container_old,
                                                    Container &         container_new)
    {
        it.end_ = tag_invoke(custom::rebind_iterator_tag{}, it.end_, container_old, container_new);
        return it;
    }

public:
    constexpr take_while_sentinel() = default;

    constexpr explicit take_while_sentinel(Sen end, Func func) :
      end_{std::move(end)}, func_{std::in_place, std::move(func)}
    {}

    template <std::forward_iterator OtherIter, std::sentinel_for<OtherIter> OtherSen>
        requires(std::convertible_to<OtherIter, Iter> && std::convertible_to<OtherSen, Sen>)
    constexpr take_while_sentinel(take_while_sentinel<OtherIter, OtherSen, Func> s) :
      end_{std::move(s.end_)}, func_{std::move(s.func_)}
    {}

    constexpr Sen const & base() const & noexcept { return end_; }
    constexpr Sen         base() && noexcept { return std::move(end_); }

    constexpr Func const & func() const & noexcept { return *func_; }
    constexpr Func         func() && { return std::move(*func_); }

    // Clang complains about the following being constexpr for some reason
    friend constexpr bool operator==(take_while_sentinel const & lhs, take_while_sentinel const & rhs) = default;

    friend constexpr bool operator==(Iter const & lhs, take_while_sentinel const & rhs)
    {
        return lhs == rhs.end_ || !std::invoke(*rhs.func_, *lhs);
    }
};

inline constexpr auto take_while_borrow = []<std::ranges::forward_range URange, typename Fn>(URange && urange, Fn fn)
{
    static_assert(borrowed_mp_range<URange>,
                  "The constraints for radr::take_while's underlying range are not satisfied.");
    static_assert(filter_func_constraints<Fn, iterator_t<URange>> &&
                    filter_func_constraints<Fn, const_iterator_t<URange>>,
                  "The constraints for radr::take_while's functor are not satisfied.");

    constexpr auto generic_impl = []<typename UIt, typename USen, typename UCIt, typename UCSen, typename Fn_>(UIt  it,
                                                                                                               USen sen,
                                                                                                               UCIt,
                                                                                                               UCSen,
                                                                                                               Fn_ _fn)
    {
        using It   = UIt;
        using Sen  = take_while_sentinel<UIt, USen, Fn_>;
        using CIt  = UCIt;
        using CSen = take_while_sentinel<UCIt, UCSen, Fn_>;

        using BorrowingRad = borrowing_rad<It, Sen, CIt, CSen, borrowing_rad_kind::unsized>;

        return BorrowingRad{
          std::move(it),
          Sen{std::move(sen), std::move(_fn)}
        };
    };

    constexpr auto folding_impl =
      []<typename UIt, typename USen, typename UCIt, typename UCSen, typename UFn, typename Fn_>(
        UIt                                   it,
        take_while_sentinel<UIt, USen, UFn>   sen,
        UCIt                                  cit,
        take_while_sentinel<UCIt, UCSen, UFn> csen,
        Fn_                                   _fn)
    {
        return decltype(generic_impl){}(std::move(it),
                                        std::move(sen).base(),
                                        std::move(cit),
                                        std::move(csen).base(),
                                        and_fn{std::move(sen).func(), std::move(_fn)});
    };

    // dispatch between generic case and chained case(s)
    return overloaded{
      generic_impl,
      folding_impl}(radr::begin(urange), radr::end(urange), radr::cbegin(urange), radr::cend(urange), std::move(fn));
};

inline constexpr auto take_while_coro = []<std::ranges::input_range URange, typename Fn>(URange && urange, Fn fn)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    static_assert(weak_indirect_unary_invocable<Fn, std::ranges::iterator_t<URange>>,
                  "The constraints for radr::take_while's functor are not satisfied.");

    // we need to create inner functor so that it can take by value
    return [](auto urange_,
              Fn   fn_) -> radr::generator<std::ranges::range_reference_t<URange>, std::ranges::range_value_t<URange>>
    {
        for (auto && elem : urange_)
        {
            if (fn_(elem))
                co_yield elem;
            else
                co_return;
        }
    }(std::move(urange), std::move(fn));
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Take elements from the prefix of a range while the filter predicate holds true.
 * \param urange The underlying range.
 * \param[in] predicate The predicate to filter by.
 * \tparam URange Type of \p urange.
 * \tparam Predicate Type of \p predicate.
 *
 * ### Multi-pass adaptor
 *
 * Requirements:
 *   * `radr::mp_range<URange>`
 *   * `std::indirect_unary_predicate<Predicate const &, radr::iterator_t<URange>>`
 *   * `std::indirect_unary_predicate<Predicate const &, radr::const_iterator_t<URange>>`
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::contiguous_range
 *   * std::ranges::borrowed_range
 *   * radr::constant_range
 *   * radr::mutable_range
 *   * radr::iterator_t and radr::const_iterator_t
 *
 * It does not preserve:
 *   * radr::common_range
 *   * std::ranges::sized_range
 *   * radr::sentinel_t and radr::const_sentinel_t
 *
 * In combination with radr::to_common, you can recreate a range of the underlying iterator and sentinel types:
 *
 * ```cpp
 * auto s = "fooBAR"sv | radr::take_while(is_lowercase) | radr::to_common;
 * // s is a string_view
 * ```
 *
 * Multiple chained take_while adaptors are folded into one.
 *
 * ### Single-pass adaptor
 *
 * Requirements:
 *   * `std::ranges::input_range<URange>`
 *   * `radr::weak_indirect_unary_invocable<Predicate, radr::iterator_t<URange>>`
 *
 */

inline constexpr auto take_while = detail::pipe_with_args_fn{detail::take_while_coro, detail::take_while_borrow};
} // namespace cpo
} // namespace radr
