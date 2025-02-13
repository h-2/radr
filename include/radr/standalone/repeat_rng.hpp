// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2025 Hannes Hauswedell
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>
#include <type_traits>

#include "radr/detail/detail.hpp"
#include "radr/detail/semiregular_box.hpp"
#include "radr/rad_util/rad_interface.hpp"
#include "radr/standalone/single_rng.hpp"

namespace radr::detail
{

template <typename T>
concept bound_reqs = one_of<T, ptrdiff_t, std::unreachable_sentinel_t>;

template <typename T>
using deduce_bound_t = std::conditional_t<std::integral<T>, ptrdiff_t, T>;

template <typename Value, bound_reqs Bound, single_rng_storage storage>
    requires std::is_object_v<Value>
class repeat_iterator
{
    using TValNoConst = std::remove_const_t<Value>;
    static_assert(storage == single_rng_storage::in_iterator || (std::is_nothrow_default_constructible_v<TValNoConst> &&
                                                                 std::is_nothrow_copy_constructible_v<TValNoConst>),
                  "The value type most be nothrow copy and default constructible.");

    using storage_type = std::conditional_t<storage == single_rng_storage::in_iterator, TValNoConst, Value *>;

    [[no_unique_address]] storage_type value{};
    [[no_unique_address]] ptrdiff_t    current_{};

public:
    using iterator_category = std::conditional_t<storage == single_rng_storage::in_iterator,
                                                 std::input_iterator_tag,
                                                 std::random_access_iterator_tag>;
    using iterator_concept  = std::random_access_iterator_tag;
    using value_type        = TValNoConst;
    using difference_type   = ptrdiff_t;

    /*!\name Constructors, destructors and assignment
     * \{
     */
    constexpr repeat_iterator()                                    = default;
    constexpr repeat_iterator(repeat_iterator &&)                  = default;
    constexpr repeat_iterator(repeat_iterator const &)             = default;
    constexpr repeat_iterator & operator=(repeat_iterator &&)      = default;
    constexpr repeat_iterator & operator=(repeat_iterator const &) = default;

    constexpr repeat_iterator(storage_type value_, ptrdiff_t bound_) :
      value{std::move(value_)}, current_{std::move(bound_)}
    {}

    //!\brief Non-const iterator to const_iterator.
    template <std::same_as<TValNoConst> OtherVal>
        requires(!std::same_as<TValNoConst, Value>)
    constexpr repeat_iterator(repeat_iterator<OtherVal, Bound, storage> it) : value{it.value}, current_{it.current_}
    {}
    //!\}

    /*!\name Forward operators
     * \{
     */

    constexpr Value & operator*() const noexcept { return *value; }

    constexpr TValNoConst operator*() const noexcept
        requires(storage == single_rng_storage::in_iterator)
    {
        return value;
    }

    constexpr repeat_iterator & operator++() noexcept
    {
        ++current_;
        return *this;
    }

    constexpr repeat_iterator operator++(int) noexcept
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }
    //!\}

    /*!\name Bidirectional operators
     * \{
     */
    constexpr repeat_iterator & operator--() noexcept
    {
        --current_;
        return *this;
    }

    constexpr repeat_iterator operator--(int) noexcept
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }
    //!\}

    /*!\name Random access operators
     * \{
     */
    constexpr repeat_iterator & operator+=(difference_type n) noexcept
    {
        current_ += n;
        return *this;
    }

    constexpr repeat_iterator & operator-=(difference_type n) noexcept
    {
        current_ -= n;
        return *this;
    }

    constexpr decltype(auto) operator[](difference_type n) const noexcept
    {
        auto tmp = *this;
        return *(tmp += n);
    }

    friend constexpr repeat_iterator operator+(repeat_iterator i, difference_type n) noexcept { return (i += n); }

    friend constexpr repeat_iterator operator+(difference_type n, repeat_iterator i) noexcept { return (i += n); }

    friend constexpr repeat_iterator operator-(repeat_iterator i, difference_type n) noexcept { return (i -= n); }

    friend constexpr difference_type operator-(repeat_iterator const & lhs, repeat_iterator const & rhs) noexcept
    {
        return static_cast<difference_type>(lhs.current_ - rhs.current_);
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    constexpr friend bool operator==(repeat_iterator const & lhs, repeat_iterator const & rhs) noexcept
    {
        return lhs.current_ == rhs.current_;
    }

    constexpr friend auto operator<=>(repeat_iterator const & lhs, repeat_iterator const & rhs) noexcept
    {
        return lhs.current_ <=> rhs.current_;
    }
    //!\}
};

} // namespace radr::detail

// GCC < 14 runs into recursive concept checks without this ¯\_(ツ)_/¯
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 14
template <typename Value, typename Bound, radr::single_rng_storage storage>
struct std::iterator_traits<radr::detail::repeat_iterator<Value, Bound, storage>>
{
    using difference_type   = ptrdiff_t;
    using value_type        = std::remove_const_t<Value>;
    using pointer           = void;
    using reference         = std::conditional_t<storage == radr::single_rng_storage::in_iterator, value_type, Value *>;
    using iterator_category = std::random_access_iterator_tag;
};
#endif

namespace radr
{

/*!\brief A range of a (possibl infinitely) repeated value.
 * \tparam TVal The value type. Must be an object type (`const` allowed, reference not).
 * \tparam Bound Either `ptrdiff_t` or std::unreachable_sentinel_t.
 * \tparam storage Storage behaviour of the value; see radr::single_rng_storage.
 *
 * \details
 *
 * |                       |  indirect      | in_range       | in_iterator    |
 * |-----------------------|----------------|----------------|----------------|
 * | category              | random_access  | random_access  | random_access  |
 * | borrowed_range        | yes            | no             | yes            |
 * | default-constructible | yes¹           | yes            | yes            |
 * | range_reference_t     | `TVal &`       | `TVal &`       | `TVal`         |
 *
 * * All variants of this range are radr::const_iterable.
 * * `in_iterator` specialisations are a std::ranges::constant_range and radr::const_symmetric.
 * * Only bounded specialisations are a std::ranges::sized_range and a radr::common_range.
 *
 * ¹ Dereferecing the iterator of a default-constructed unbounded repeat_rng with indirect storage is
 * undefined behaviour.
 */
template <typename TVal,
          typename Bound             = std::unreachable_sentinel_t,
          single_rng_storage storage = single_rng_storage::in_range>
    requires(std::is_object_v<TVal> && detail::bound_reqs<Bound>)
class repeat_rng : public rad_interface<repeat_rng<TVal, Bound, storage>>
{
    using TValNoConst = std::remove_const_t<TVal>;

    static_assert(storage == single_rng_storage::indirect || std::copy_constructible<TValNoConst>,
                  "The value type needs to be copy constructible.");

    static_assert(storage == single_rng_storage::indirect || std::default_initializable<TValNoConst>,
                  "The value type needs to be default-initializable.");

    static_assert(storage != single_rng_storage::in_iterator || (std::is_nothrow_default_constructible_v<TValNoConst> &&
                                                                 std::is_nothrow_copy_constructible_v<TValNoConst>),
                  "The value type needs to be nothrow_default_constructible and nothrow_copy_constructible for "
                  "in_iterator storage.");

    using storage_type =
      std::conditional_t<storage == single_rng_storage::indirect, TVal *, detail::semiregular_box<TVal>>;

    [[no_unique_address]] storage_type value{};
    [[no_unique_address]] Bound        bound_ = Bound();

    auto get_value()
    {
        if constexpr (storage == single_rng_storage::indirect)
            return value;
        else if constexpr (storage == single_rng_storage::in_range)
            return &*value;
        else
            return *value;
    }

    auto get_value() const // the centre-line is different between const and non-const
    {
        if constexpr (storage == single_rng_storage::indirect)
            return value;
        else if constexpr (storage == single_rng_storage::in_range)
            return &*value;
        else
            return *value;
    }

    static constexpr bool const_symmetric = std::is_const_v<TVal> || storage == single_rng_storage::in_iterator;

public:
    /*!\name Constructors: Rule-of-5
     * \{
     */
    constexpr repeat_rng()                               = default;
    constexpr repeat_rng(repeat_rng const &)             = default;
    constexpr repeat_rng(repeat_rng &&)                  = default;
    constexpr repeat_rng & operator=(repeat_rng const &) = default;
    constexpr repeat_rng & operator=(repeat_rng &&)      = default;
    //!\}

    /*!\name Constructors for indirect storage
     * \{
     */
    constexpr explicit repeat_rng(TVal & val, Bound b = Bound())
        requires(storage == single_rng_storage::indirect)
      : value(&val), bound_(std::move(b))
    {}

    constexpr explicit repeat_rng(std::reference_wrapper<TVal> const & val, Bound b = Bound())
        requires(storage == single_rng_storage::indirect)
      : value(&static_cast<TVal &>(val)), bound_(std::move(b))
    {}

    constexpr explicit repeat_rng(TVal && val, Bound b = Bound())
        requires(storage == single_rng_storage::indirect)
    = delete;
    //!\}

    /*!\name Constructors for in_range or in_iterator storage
     * \{
     */
    constexpr explicit repeat_rng(TVal val, Bound b = Bound())
        requires(storage != single_rng_storage::indirect)
      : value(std::in_place, std::move(val)), bound_(std::move(b))
    {}

    template <class... TArgs, class... BoundArgs>
        requires std::constructible_from<TValNoConst, TArgs...> && std::constructible_from<Bound, BoundArgs...> &&
                   (storage != single_rng_storage::indirect)
    constexpr explicit repeat_rng(std::piecewise_construct_t,
                                  std::tuple<TArgs...>     value_args,
                                  std::tuple<BoundArgs...> bound_args = std::tuple<>{}) :
      value{std::make_from_tuple<TValNoConst>(std::move(value_args))},
      bound_{std::make_from_tuple<Bound>(std::move(bound_args))}
    {}
    //!\}

    /*!\name Range access
     * \{
     */
    constexpr auto begin() noexcept
        requires(!const_symmetric)
    {
        return detail::repeat_iterator<TVal, Bound, storage>{get_value(), 0};
    }

    constexpr auto begin() const noexcept
    {
        return detail::repeat_iterator<TVal const, Bound, storage>{get_value(), 0};
    }

    constexpr auto end() noexcept
        requires(!std::same_as<Bound, std::unreachable_sentinel_t> && !const_symmetric)
    {
        return detail::repeat_iterator<TVal, Bound, storage>{get_value(), bound_};
    }

    constexpr auto end() const noexcept
        requires(!std::same_as<Bound, std::unreachable_sentinel_t>)
    {
        return detail::repeat_iterator<TVal const, Bound, storage>{get_value(), bound_};
    }

    constexpr std::unreachable_sentinel_t end() const noexcept { return std::unreachable_sentinel; }
    //!\}

    /*!\name Observers
     * \{
     */
    static constexpr bool empty() noexcept
        requires(std::same_as<Bound, std::unreachable_sentinel_t>)
    {
        return false;
    }

    using rad_interface<repeat_rng<TVal, Bound, storage>>::empty;

    /*!\brief Compares the value and the size.
     * \pre Neither side is an unbounded **and** indirect repeat_rng that is default-initialised.
     */
    constexpr friend bool operator==(repeat_rng const & lhs, repeat_rng const & rhs)
        requires detail::weakly_equality_comparable<TVal>
    {
        if constexpr (std::same_as<Bound, std::unreachable_sentinel_t>)
            return lhs.front() == rhs.front();
        else
            return lhs.bound_ == rhs.bound_ && lhs.bound_ > 0 && lhs.front() == rhs.front();
    }
    //!\}
};

/*!\name Deduction guides
 * \{
 */
//!\brief Default is in_range storage.
template <typename T>
repeat_rng(T &&) -> repeat_rng<std::remove_reference_t<T>, std::unreachable_sentinel_t, single_rng_storage::in_range>;
//!\brief Default is in_range storage.
template <typename T, typename Bounds>
repeat_rng(T &&, Bounds)
  -> repeat_rng<std::remove_reference_t<T>, detail::deduce_bound_t<Bounds>, single_rng_storage::in_range>;

//!\brief Small types default to in_iterator storage.
template <typename T>
    requires small_type<std::remove_cvref_t<T>>
repeat_rng(T &&)
  -> repeat_rng<std::remove_reference_t<T>, std::unreachable_sentinel_t, single_rng_storage::in_iterator>;
//!\brief Small types default to in_iterator storage.
template <typename T, typename Bounds>
    requires small_type<std::remove_cvref_t<T>>
repeat_rng(T &&, Bounds)
  -> repeat_rng<std::remove_reference_t<T>, detail::deduce_bound_t<Bounds>, single_rng_storage::in_iterator>;

//!\brief Reference wrapper lead to indirect storage.
template <typename T>
repeat_rng(std::reference_wrapper<T> &&) -> repeat_rng<T, std::unreachable_sentinel_t, single_rng_storage::indirect>;
//!\brief Reference wrapper lead to indirect storage.
template <typename T>
repeat_rng(std::reference_wrapper<T> &) -> repeat_rng<T, std::unreachable_sentinel_t, single_rng_storage::indirect>;
//!\brief Reference wrapper lead to indirect storage.
template <typename T>
repeat_rng(std::reference_wrapper<T> const &)
  -> repeat_rng<T, std::unreachable_sentinel_t, single_rng_storage::indirect>;
//!\brief Reference wrapper lead to indirect storage.
template <typename T, typename Bounds>
repeat_rng(std::reference_wrapper<T> &&, Bounds)
  -> repeat_rng<T, detail::deduce_bound_t<Bounds>, single_rng_storage::indirect>;
//!\brief Reference wrapper lead to indirect storage.
template <typename T, typename Bounds>
repeat_rng(std::reference_wrapper<T> &, Bounds)
  -> repeat_rng<T, detail::deduce_bound_t<Bounds>, single_rng_storage::indirect>;
//!\brief Reference wrapper lead to indirect storage.
template <typename T, typename Bounds>
repeat_rng(std::reference_wrapper<T> const &, Bounds)
  -> repeat_rng<T, detail::deduce_bound_t<Bounds>, single_rng_storage::indirect>;
//!\}

} // namespace radr

template <class T, typename Bound, radr::single_rng_storage storage>
inline constexpr bool std::ranges::enable_borrowed_range<radr::repeat_rng<T, Bound, storage>> =
  (storage != radr::single_rng_storage::in_range);
