// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <concepts>
#include <optional>

// copyable_box allows turning a type that is copy-constructible (but maybe not copy-assignable) into
// a type that is both copy-constructible and copy-assignable. It does that by introducing an empty state
// and basically doing destroy-then-copy-construct in the assignment operator. The empty state is necessary
// to handle the case where the copy construction fails after destroying the object.
//
// In some cases, we can completely avoid the use of an empty state; we provide a specialization of
// copyable_box that does this, see below for the details.

namespace radr::detail
{

template <class Tp>
concept copy_constructible_object = std::copy_constructible<Tp> && std::is_object_v<Tp>;

}

namespace radr
{
// Primary template - uses std::optional and introduces an empty state in case assignment fails.
template <detail::copy_constructible_object Tp>
class copyable_box
{
    [[no_unique_address]] std::optional<Tp> val_;

public:
    template <class... Args>
        requires std::is_constructible_v<Tp, Args...>
    constexpr explicit copyable_box(std::in_place_t,
                                    Args &&... args) noexcept(std::is_nothrow_constructible_v<Tp, Args...>) :
      val_(std::in_place, std::forward<Args>(args)...)
    {}

    constexpr copyable_box() noexcept(std::is_nothrow_default_constructible_v<Tp>)
        requires std::default_initializable<Tp>
      : val_(std::in_place)
    {}

    copyable_box(copyable_box const &) = default;
    copyable_box(copyable_box &&)      = default;

    constexpr copyable_box & operator=(copyable_box const & other) noexcept(std::is_nothrow_copy_constructible_v<Tp>)
    {
        if (this != std::addressof(other))
        {
            if (other.has_value())
                val_.emplace(*other);
            else
                val_.reset();
        }
        return *this;
    }

    copyable_box & operator=(copyable_box &&)
        requires std::movable<Tp>
    = default;

    constexpr copyable_box & operator=(copyable_box && other) noexcept(std::is_nothrow_move_constructible_v<Tp>)
    {
        if (this != std::addressof(other))
        {
            if (other.has_value())
                val_.emplace(std::move(*other));
            else
                val_.reset();
        }
        return *this;
    }

    constexpr Tp const & operator*() const noexcept { return *val_; }
    constexpr Tp &       operator*() noexcept { return *val_; }

    constexpr Tp const * operator->() const noexcept { return val_.operator->(); }
    constexpr Tp *       operator->() noexcept { return val_.operator->(); }

    constexpr bool has_value() const noexcept { return val_.has_value(); }
};

} // namespace radr

// This partial specialization implements an optimization for when we know we don't need to store
// an empty state to represent failure to perform an assignment. For copy-assignment, this happens:
//
// 1. If the type is copyable (which includes copy-assignment), we can use the type's own assignment operator
//    directly and avoid using std::optional.
// 2. If the type is not copyable, but it is nothrow-copy-constructible, then we can implement assignment as
//    destroy-and-then-construct and we know it will never fail, so we don't need an empty state.
//
// The exact same reasoning can be applied for move-assignment, with copyable replaced by std::movable and
// nothrow-copy-constructible replaced by nothrow-move-constructible. This specialization is enabled
// whenever we can apply any of these optimizations for both the copy assignment and the move assignment
// operator.

namespace radr::detail
{

template <class Tp>
concept doesnt_need_empty_state_for_copy = std::copyable<Tp> || std::is_nothrow_copy_constructible_v<Tp>;

template <class Tp>
concept doesnt_need_empty_state_for_move = std::movable<Tp> || std::is_nothrow_move_constructible_v<Tp>;

} // namespace radr::detail

namespace radr
{

template <detail::copy_constructible_object Tp>
    requires detail::doesnt_need_empty_state_for_copy<Tp> && detail::doesnt_need_empty_state_for_move<Tp>
class copyable_box<Tp>
{
    [[no_unique_address]] Tp val_;

public:
    template <class... Args>
        requires std::is_constructible_v<Tp, Args...>
    constexpr explicit copyable_box(std::in_place_t,
                                    Args &&... args) noexcept(std::is_nothrow_constructible_v<Tp, Args...>) :
      val_(std::forward<Args>(args)...)
    {}

    constexpr copyable_box() noexcept(std::is_nothrow_default_constructible_v<Tp>)
        requires std::default_initializable<Tp>
      : val_()
    {}

    copyable_box(copyable_box const &) = default;
    copyable_box(copyable_box &&)      = default;

    // Implementation of assignment operators in case we perform optimization (1)
    copyable_box & operator=(copyable_box const &)
        requires std::copyable<Tp>
    = default;
    copyable_box & operator=(copyable_box &&)
        requires std::movable<Tp>
    = default;

    // Implementation of assignment operators in case we perform optimization (2)
    constexpr copyable_box & operator=(copyable_box const & other) noexcept
    {
        static_assert(std::is_nothrow_copy_constructible_v<Tp>);
        if (this != std::addressof(other))
        {
            std::destroy_at(std::addressof(val_));
            std::construct_at(std::addressof(val_), other.val_);
        }
        return *this;
    }

    constexpr copyable_box & operator=(copyable_box && other) noexcept
    {
        static_assert(std::is_nothrow_move_constructible_v<Tp>);
        if (this != std::addressof(other))
        {
            std::destroy_at(std::addressof(val_));
            std::construct_at(std::addressof(val_), std::move(other.val_));
        }
        return *this;
    }

    constexpr Tp const & operator*() const noexcept { return val_; }
    constexpr Tp &       operator*() noexcept { return val_; }

    constexpr Tp const * operator->() const noexcept { return std::addressof(val_); }
    constexpr Tp *       operator->() noexcept { return std::addressof(val_); }

    constexpr bool has_value() const noexcept { return true; }
};
} // namespace radr
