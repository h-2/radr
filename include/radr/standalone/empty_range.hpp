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
#include <ranges>
#include <type_traits>

#include "radr/custom/tags.hpp"
#include "radr/detail/detail.hpp"
#include "radr/rad_util/rad_interface.hpp"

namespace radr
{

template <class T>
    requires std::is_object_v<T>
class empty_range : public rad_interface<empty_range<T>>
{
public:
    constexpr T * begin() noexcept
        requires(!std::is_const_v<T>)
    {
        return nullptr;
    }
    constexpr T * end() noexcept
        requires(!std::is_const_v<T>)
    {
        return nullptr;
    }
    constexpr T * data() noexcept
        requires(!std::is_const_v<T>)
    {
        return nullptr;
    }
    constexpr T const * begin() const noexcept { return nullptr; }
    constexpr T const * end() const noexcept { return nullptr; }
    constexpr T const * data() const noexcept { return nullptr; }

    static constexpr size_t size() noexcept { return 0; }
    static constexpr bool   empty() noexcept { return true; }

    constexpr friend bool operator==(empty_range const &, empty_range const &) { return true; }

    template <detail::decays_to<empty_range> R, detail::decays_to<std::remove_const_t<T>> T_>
    constexpr friend auto tag_invoke(custom::subborrow_tag, R &&, T_ *, T_ *)
    {
        return empty_range<T_>{};
    }

    template <detail::decays_to<empty_range> R, detail::decays_to<std::remove_const_t<T>> T_>
    constexpr friend auto tag_invoke(custom::subborrow_tag, R &&, T_ *, T_ *, size_t)
    {
        return empty_range<T_>{};
    }
};

} // namespace radr

template <class T>
inline constexpr bool std::ranges::enable_borrowed_range<radr::empty_range<T>> = true;
