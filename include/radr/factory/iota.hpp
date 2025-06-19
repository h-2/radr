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

#include <concepts>
#include <istream>
#include <iterator>

#include "radr/concepts.hpp"
#include "radr/detail/fwd.hpp"
#include "radr/generator.hpp"
#include "radr/rad_util/borrowing_rad.hpp"

namespace radr::detail
{

// clang-format off
template <typename Int>
using wider_signed_t = std::conditional_t<sizeof(Int) < sizeof(short), short,
                       std::conditional_t<sizeof(Int) < sizeof(int),   int,
                       std::conditional_t<sizeof(Int) < sizeof(long),  long,
                                                                       long long>>>;
// clang-format on

template <class Start>
using iota_diff_t =
  std::conditional_t<(!std::is_integral_v<Start> || sizeof(std::iter_difference_t<Start>) > sizeof(Start)),
                     std::iter_difference_t<Start>,
                     wider_signed_t<Start>>;

template <class Iter>
concept decrementable = std::incrementable<Iter> && requires(Iter i) {
    {
        --i
    } -> std::same_as<Iter &>;
    {
        i--
    } -> std::same_as<Iter>;
};

template <class Iter>
concept advanceable =
  decrementable<Iter> && std::totally_ordered<Iter> && requires(Iter i, Iter const j, iota_diff_t<Iter> const n) {
      {
          i += n
      } -> std::same_as<Iter &>;
      {
          i -= n
      } -> std::same_as<Iter &>;
      Iter(j + n);
      Iter(n + j);
      Iter(j - n);
      {
          j - j
      } -> std::convertible_to<iota_diff_t<Iter>>;
  };

template <std::incrementable Start>
class iota_iterator
{
private:
    Start value_{};

public:
    using value_type        = Start;
    using difference_type   = iota_diff_t<value_type>;
    using iterator_category = std::input_iterator_tag;
    // clang-format off
    using iterator_concept  =
      std::conditional_t<advanceable<value_type>,   std::random_access_iterator_tag,
      std::conditional_t<decrementable<value_type>, std::bidirectional_iterator_tag,
                                                    std::forward_iterator_tag>>;
    // clang-format on

    constexpr iota_iterator() = default;
    constexpr explicit iota_iterator(Start value) : value_(std::move(value)) {}

    constexpr value_type operator*() const noexcept(std::is_nothrow_copy_constructible_v<value_type>) { return value_; }
    constexpr iota_iterator & operator++()
    {
        ++value_;
        return *this;
    }
    constexpr iota_iterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr iota_iterator & operator--()
        requires decrementable<value_type>
    {
        --value_;
        return *this;
    }
    constexpr iota_iterator operator--(int)
        requires decrementable<value_type>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr iota_iterator & operator+=(difference_type n)
        requires advanceable<value_type>
    {
        if constexpr (std::is_integral_v<value_type> && !std::is_signed_v<value_type>)
        {
            if (n >= difference_type(0))
                value_ += static_cast<value_type>(n);
            else
                value_ -= static_cast<value_type>(-n);
        }
        else
        {
            value_ += n;
        }
        return *this;
    }

    constexpr iota_iterator & operator-=(difference_type n)
        requires advanceable<value_type>
    {
        if constexpr (std::is_integral_v<value_type> && !std::is_signed_v<value_type>)
        {
            if (n >= difference_type(0))
                value_ -= static_cast<value_type>(n);
            else
                value_ += static_cast<value_type>(-n);
        }
        else
        {
            value_ -= n;
        }
        return *this;
    }

    constexpr value_type operator[](difference_type n) const
        requires advanceable<value_type>
    {
        return Start(value_ + n);
    }

    friend constexpr bool operator==(iota_iterator const & x, iota_iterator const & y)
        requires std::equality_comparable<value_type>
    {
        return x.value_ == y.value_;
    }

    friend constexpr bool operator<(iota_iterator const & x, iota_iterator const & y)
        requires std::totally_ordered<value_type>
    {
        return x.value_ < y.value_;
    }

    friend constexpr bool operator>(iota_iterator const & x, iota_iterator const & y)
        requires std::totally_ordered<value_type>
    {
        return y < x;
    }

    friend constexpr bool operator<=(iota_iterator const & x, iota_iterator const & y)
        requires std::totally_ordered<value_type>
    {
        return !(y < x);
    }

    friend constexpr bool operator>=(iota_iterator const & x, iota_iterator const & y)
        requires std::totally_ordered<value_type>
    {
        return !(x < y);
    }

    friend constexpr auto operator<=>(iota_iterator const & x, iota_iterator const & y)
        requires std::totally_ordered<value_type> && std::three_way_comparable<value_type>
    {
        return x.value_ <=> y.value_;
    }

    friend constexpr iota_iterator operator+(iota_iterator i, difference_type n)
        requires advanceable<value_type>
    {
        i += n;
        return i;
    }

    friend constexpr iota_iterator operator+(difference_type n, iota_iterator i)
        requires advanceable<value_type>
    {
        return i + n;
    }

    friend constexpr iota_iterator operator-(iota_iterator i, difference_type n)
        requires advanceable<value_type>
    {
        i -= n;
        return i;
    }

    friend constexpr difference_type operator-(iota_iterator const & x, iota_iterator const & y)
        requires advanceable<value_type>
    {
        if constexpr (std::is_integral_v<value_type>)
        {
            if constexpr (std::is_signed_v<value_type>)
            {
                return difference_type(difference_type(x.value_) - difference_type(y.value_));
            }

            if (y.value_ > x.value_)
                return difference_type(-difference_type(y.value_ - x.value_));

            return difference_type(x.value_ - y.value_);
        }
        return x.value_ - y.value_;
    }
};

template <typename Start, typename Bound>
class iota_sentinel
{
private:
    Bound bound_{};

public:
    constexpr iota_sentinel() = default;
    constexpr explicit iota_sentinel(Bound b) : bound_(std::move(b)) {}

    friend constexpr bool operator==(iota_iterator<Start> const & it, iota_sentinel const & sent)
    {
        return *it == sent.bound_;
    }
    friend constexpr auto operator-(iota_iterator<Start> const & it, iota_sentinel const & sent)
        requires std::sized_sentinel_for<Bound, Start>
    {
        return *it - sent.bound_;
    }
    friend constexpr auto operator-(iota_sentinel const & sent, iota_iterator<Start> const & it)
        requires std::sized_sentinel_for<Bound, Start>
    {
        return -(it - sent);
    }
};

struct iota_fn
{
    template <typename Value, typename Bound = std::unreachable_sentinel_t>
    constexpr auto operator()(Value val, Bound bound = std::unreachable_sentinel) const
    {
        static_assert(std::incrementable<Value>, "The Value type to radr::iota needs to satisfy std::incrementable.");
        static_assert(weakly_equality_comparable_with<Value, Bound>,
                      "The Value type to radr::iota needs to be comparable with the Bound type.");
        static_assert(std::semiregular<Bound>, "The Bound type to radr::iota needs to satisfy std::regular.");

        using It  = detail::iota_iterator<Value>;
        using Sen = std::conditional_t<std::same_as<Value, Bound>, It, detail::iota_sentinel<Value, Bound>>;

        constexpr borrowing_rad_kind kind =
          ((std::random_access_iterator<It> && std::same_as<It, Sen>) || std::sized_sentinel_for<Bound, Value>)
            ? borrowing_rad_kind::sized
            : borrowing_rad_kind::unsized;

        return borrowing_rad<It, Sen, It, Sen, kind>{It{val}, Sen{bound}};
    }
};

} // namespace radr::detail

namespace radr
{

/*!\brief A range factory that generates a sequence of elements by repeatedly incrementing an initial value. Can be either bounded or unbounded (infinite).
 * \tparam Value The type of \p value; required to be std::incrementable.
 * \tparam Bound The type of \p bound; required to be std::semiregular and detail::weakly_equality_comparable_with Value.
 * \param[in] value The initial value.
 * \param[in] bound The bound; std::unreachable_sentinel by default.
 * \details
 *
 * In contrast to std::views::iota, this factory always returns a multi-pass range and thus requires
 * `std::incrementable<Value>` and not just `std::weakly_incrementable<Value>`.
 *
 * There is radr::iota_sp which is always a single-pass range and does not have this requirement.
 *
 */
inline constexpr detail::iota_fn iota{};

/*!\brief Single-pass version of radr::iota.
 * \tparam Value The type of \p value; required to be std::weakly_incrementable.
 * \tparam Bound The type of \p bound; required to be detail::weakly_equality_comparable_with Value.
 * \param[in] value The initial value.
 * \param[in] bound The bound; std::unreachable_sentinel by default.
 * \details
 *
 * This factory always returns a radr::generator, a move-only, single-pass range.
 *
 * The requirements on the types are weaker than for radr::iota.
 */
inline constexpr auto iota_sp =
  []<typename Value, typename Bound = std::unreachable_sentinel_t>(Value val, Bound bound = {})->radr::generator<Value>
{
    static_assert(std::weakly_incrementable<Value>,
                  "The Value type to radr::iota_sp needs to satisfy std::weakly_incrementable.");
    static_assert(detail::weakly_equality_comparable_with<Value, Bound>,
                  "The Value type to radr::iota_sp needs to be comparable with the Bound type.");

    while (val != bound)
    {
        co_yield val;
        ++val;
    }
};

} // namespace radr
