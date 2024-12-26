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

#include <compare>
#include <concepts>
#include <memory>

namespace radr::detail
{

template <std::copyable T>
class indirect
{
private:
    std::unique_ptr<T> data;

public:
    constexpr indirect() noexcept
        requires std::default_initializable<T>
      : data{new T{}}
    {}
    //!\brief This is different from the proposed one but useful for us.
    constexpr indirect() noexcept
        requires(!std::default_initializable<T>)
    = default;

    constexpr indirect(indirect &&) noexcept             = default;
    constexpr indirect & operator=(indirect &&) noexcept = default;

    constexpr indirect(indirect const & rhs) { *this = rhs; }

    constexpr indirect & operator=(indirect const & rhs)
    {
        if (data == rhs.data)
        {
        }
        else if (rhs.data == nullptr)
            data.reset(nullptr);
        else if (data)
            *data = *rhs.data;
        else
            data.reset(new T(*rhs.data));

        return *this;
    }

    constexpr indirect(std::convertible_to<T> auto && val) : data{new T(std::forward<decltype(val)>(val))} {}

    constexpr T & operator*() & noexcept
    {
        assert(data);
        return *data;
    }

    constexpr T const & operator*() const & noexcept
    {
        assert(data);
        return *data;
    }

    constexpr T && operator*() && noexcept
    {
        assert(data);
        return std::move(*data);
    }

    constexpr T * get() const noexcept { return data.get(); }

    constexpr friend auto operator<=>(indirect const & lhs, indirect const & rhs)
        requires std::three_way_comparable<T>
    {
        assert(lhs.data);
        assert(rhs.data);
        return *lhs <=> *rhs;
    }

    constexpr friend bool operator==(indirect const & lhs, indirect const & rhs)
        requires std::equality_comparable<T>
    {
        assert(lhs.data);
        assert(rhs.data);
        return *lhs == *rhs;
    }
};

} // namespace radr::detail
