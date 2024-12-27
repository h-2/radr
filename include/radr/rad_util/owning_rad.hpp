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

#include <any>
#include <memory>
#include <ranges>

#include "../concepts.hpp"
#include "../custom/tags.hpp"
#include "../detail/detail.hpp"
#include "../detail/indirect.hpp"
#include "../generator.hpp"
#include "rad_interface.hpp"
#include "radr/custom/subborrow.hpp"

/* TODO add version that stores offsets for RA+sized uranges
 * this avoids unique_ptr and BorrowedRange
 * NO; THIS WILL BE SEPARATE
 *
 */

namespace radr::detail
{

template <typename Range>
concept owned_range_constraints =
  unqualified_forward_range<Range> && const_iterable_range<Range> && std::copyable<Range>;

template <typename Range>
concept borrowed_range_constraints =
  owned_range_constraints<Range> && std::default_initializable<Range> && std::ranges::borrowed_range<Range>;

template <typename BorrowedRange, typename Container>
concept rebindable = requires ( BorrowedRange const & b, Container const & u)
        {
            { rebind(b, u, u) } -> std::same_as<BorrowedRange>;
        };

template <typename URange>
constexpr URange & unpackAny(std::any const & a)
{
    return *(std::any_cast<detail::indirect<URange>>(&a)->get());
}

//     template <typename URange, typename BorrowedRange>
//     constexpr BorrowedRange rebind_fn(BorrowedRange const & oldBounds,
//                                        std::any const & oldContainer,
//                                        std::any const & newContainer)
//     {
//         if constexpr (std::same_as<URange, void>)
//         {
//             return BorrowedRange{};
//         }
//         else if constexpr (rebindable<BorrowedRange, URange>)
//         {
//             // rebind() is a hidden friend of borrowing_rad
//             // using get() here intentionally bypasses deep-const of indirect()
//             // static_assert(std::same_as<decltype(std::any_cast<detail::indirect<URange>>(&oldContainer)),
//             //                            detail::indirect<URange> const *>);
//             //
//             // static_assert(std::same_as<decltype(std::any_cast<detail::indirect<URange>>(&oldContainer)->get()),
//             //                            URange *>);
//             //
//             // static_assert(std::same_as<decltype(*std::any_cast<detail::indirect<URange>>(&oldContainer)->get()),
//             //                            URange &>);
//
//             return rebind(oldBounds, unpackAny<URange>(oldContainer), unpackAny<URange>(newContainer));
//         }
//         else
//         {
//             throw std::runtime_error{"This owning rad is not copyable, because the rad's iterators could not be "
//                                      "rebound. Please see the Wiki on how to make custom iterators rebindable, "
//                                      "and/or open a bug report at https://github.com/h-2/radr ."};
//         }
//     }

template <typename URange>
struct rebind_fn
{
    template <typename BorrowedRange>
    constexpr BorrowedRange operator()(BorrowedRange const & oldBounds,
                                       std::any const & oldContainer,
                                       std::any const & newContainer) const
    {
        if constexpr (std::same_as<URange, void>)
        {
            return BorrowedRange{};
        }
        else if constexpr (rebindable<BorrowedRange, URange>)
        {
            // rebind() is a hidden friend of borrowing_rad
            // using get() here intentionally bypasses deep-const of indirect()
            // static_assert(std::same_as<decltype(std::any_cast<detail::indirect<URange>>(&oldContainer)),
            //                            detail::indirect<URange> const *>);
            //
            // static_assert(std::same_as<decltype(std::any_cast<detail::indirect<URange>>(&oldContainer)->get()),
            //                            URange *>);
            //
            // static_assert(std::same_as<decltype(*std::any_cast<detail::indirect<URange>>(&oldContainer)->get()),
            //                            URange &>);

            return rebind(oldBounds, unpackAny<URange>(oldContainer), unpackAny<URange>(newContainer));
        }
        else
        {
            throw std::runtime_error{"This owning rad is not copyable, because the rad's iterators could not be "
                                     "rebound. Please see the Wiki on how to make custom iterators rebindable, "
                                     "and/or open a bug report at https://github.com/h-2/radr ."};
        }
    }
};


} // namespace radr::detail

namespace radr
{

template <detail::borrowed_range_constraints BorrowedRange>
class owning_rad : public rad_interface<owning_rad<BorrowedRange>>
{
    [[no_unique_address]] std::any base_{};
    [[no_unique_address]] BorrowedRange            bounds{};
    [[no_unique_address]] std::function<BorrowedRange(BorrowedRange, std::any const &, std::any const &)> rebinder
     = detail::rebind_fn<void>{};
    // [[no_unique_address]] BorrowedRange(*rebinder)(BorrowedRange, std::any const &, std::any const &)
     // = &detail::rebind_fn<void, BorrowedRange>;

    static constexpr bool const_symmetric = const_symmetric_range<BorrowedRange>;

    template <detail::borrowed_range_constraints BorrowedRange_>
    friend class owning_rad;



public:
    owning_rad()                          = default;
    owning_rad(owning_rad &&)             = default;
    owning_rad & operator=(owning_rad &&) = default;

    owning_rad(owning_rad const & rhs) { *this = rhs; }

    owning_rad & operator=(owning_rad const & rhs)
    {
        if (this != &rhs)
        {
            base_ = rhs.base_;
            rebinder = rhs.rebinder;
            bounds = rebinder(rhs.bounds, rhs.base_, base_);
        }
        return *this;
    }

    template <detail::owned_range_constraints URange>
    constexpr explicit owning_rad(URange && base) :
        base_(detail::indirect<URange>(std::move(base))),
        bounds{detail::unpackAny<URange>(base_)},
        rebinder(detail::rebind_fn<URange>{})
        // rebinder{&detail::rebind_fn<BorrowedRange, URange>}
    {}

    //TODO we want rvalue ref not forwarding ref
    template <detail::owned_range_constraints URange, typename Fn>
        requires std::regular_invocable<Fn &&, URange &>
    constexpr owning_rad(URange && base, Fn cacher_fn) :
        base_(detail::indirect<URange>(std::move(base))),
        bounds{std::move(cacher_fn)(detail::unpackAny<URange>(base_))},
        rebinder(detail::rebind_fn<URange>{})
        // rebinder{&detail::rebind_fn<BorrowedRange, URange>}
    {}

    //!\brief Collapsing constructor
    template <typename Fn, detail::different_from<BorrowedRange> BorrowedRange_>
        requires std::regular_invocable<Fn &&, BorrowedRange_ &>
    constexpr owning_rad(owning_rad<BorrowedRange_> && urange, Fn cacher_fn) :
      base_(std::move(urange.base_)), bounds{std::move(cacher_fn)(urange.bounds)},
      rebinder(reinterpret_cast<BorrowedRange(*)(BorrowedRange const &, std::any const &, std::any const &)>(&urange.rebinder))
    {

    }

    constexpr auto begin()
        requires(!const_symmetric)
    {
        return radr::begin(bounds);
    }

    constexpr auto begin() const { return radr::begin(bounds); }

    constexpr auto end()
        requires(!const_symmetric)
    {
        return radr::end(bounds);
    }

    constexpr auto end() const { return radr::end(bounds); }

    constexpr auto size()
        requires std::ranges::sized_range<BorrowedRange>
    {
        return std::ranges::size(bounds);
    }

    constexpr auto size() const
        requires std::ranges::sized_range<BorrowedRange const>
    {
        return std::ranges::size(bounds);
    }

    constexpr friend bool operator==(owning_rad const & lhs, owning_rad const & rhs)
        requires detail::weakly_equality_comparable<BorrowedRange>
    {
        return lhs.bounds == rhs.bounds;
    }
};

template <class Range>
owning_rad(Range &&)
  -> owning_rad<decltype(borrowing_rad{std::declval<std::remove_cvref_t<Range> &>()})>;

template <class Range, class CacherFn>
    requires std::regular_invocable<CacherFn &&, std::remove_cvref_t<Range> &>
owning_rad(Range &&, CacherFn)
  -> owning_rad<std::invoke_result_t<CacherFn &&, std::remove_cvref_t<Range> &>>;

template <class BorrowedRange_, class CacherFn>
    requires std::regular_invocable<CacherFn &&, std::remove_cvref_t<BorrowedRange_> &>
owning_rad(owning_rad<BorrowedRange_> &&, CacherFn)
  -> owning_rad<std::invoke_result_t<CacherFn &&, std::remove_cvref_t<BorrowedRange_> &>>;

} // namespace radr
