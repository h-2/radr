// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2024 Hannes Hauswedell
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstddef>
#include <type_traits>

#include "radr/detail/semiregular_box.hpp"
#include "radr/rad_util/rad_interface.hpp"

namespace radr
{

template <std::copy_constructible TVal>
    requires std::is_object_v<TVal>
class single : public rad_interface<single<TVal>>
{
    [[no_unique_address]] detail::semiregular_box<TVal> value;

public:
    single() = default;

    constexpr explicit single(TVal const & val) : value(std::in_place, val) {}
    constexpr explicit single(TVal && val) : value(std::in_place, std::move(val)) {}

    template <class... _Args>
        requires std::constructible_from<TVal, _Args...>
    constexpr explicit single(std::in_place_t, _Args &&... args) : value{std::in_place, std::forward<_Args>(args)...}
    {}

    constexpr TVal *       begin() noexcept { return data(); }
    constexpr TVal const * begin() const noexcept { return data(); }

    constexpr TVal *       end() noexcept { return data() + 1; }
    constexpr TVal const * end() const noexcept { return data() + 1; }

    static constexpr size_t size() noexcept { return 1; }

    constexpr TVal *       data() noexcept { return value.operator->(); }
    constexpr TVal const * data() const noexcept { return value.operator->(); }

    constexpr friend bool operator==(single const & lhs, single const & rhs)
        requires detail::weakly_equality_comparable<TVal>
    {
        return lhs.front() == rhs.front();
    }
};

} // namespace radr
