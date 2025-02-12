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

#include <concepts>
#include <cstddef>
#include <type_traits>

#include "radr/detail/semiregular_box.hpp"
#include "radr/rad_util/rad_interface.hpp"

namespace radr::detail
{

template <typename Value>
    requires(decays_to<Value, Value> && std::is_nothrow_default_constructible_v<Value> &&
             std::is_nothrow_copy_constructible_v<Value>)
class small_single_iterator
{
    [[no_unique_address]] Value value{};
    [[no_unique_address]] bool  at_end = true;

public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = Value;
    using difference_type   = ptrdiff_t;

    /*!\name Constructors, destructors and assignment
     * \{
     */
    constexpr small_single_iterator()                                          = default;
    constexpr small_single_iterator(small_single_iterator &&)                  = default;
    constexpr small_single_iterator(small_single_iterator const &)             = default;
    constexpr small_single_iterator & operator=(small_single_iterator &&)      = default;
    constexpr small_single_iterator & operator=(small_single_iterator const &) = default;

    constexpr small_single_iterator(Value value_, bool at_end_ = false) : value{std::move(value_)}, at_end{at_end_} {}

    template <class... _Args>
        requires std::constructible_from<Value, _Args...>
    constexpr explicit small_single_iterator(std::in_place_t, _Args &&... args) :
      value{std::in_place, std::forward<_Args>(args)...}
    {}
    //!\}

    /*!\name Forward operators
     * \{
     */
    constexpr Value operator*() const noexcept { return value; }

    constexpr small_single_iterator & operator++() noexcept
    {
        at_end = true;
        return *this;
    }

    constexpr small_single_iterator operator++(int) noexcept
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }
    //!\}

    /*!\name Bidirectional operators
     * \{
     */
    constexpr small_single_iterator & operator--() noexcept(noexcept(std::is_nothrow_default_constructible_v<Value>))
    {
        at_end = false;
        return *this;
    }

    constexpr small_single_iterator operator--(int) noexcept(noexcept(std::is_nothrow_default_constructible_v<Value>))
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }
    //!\}

    /*!\name Random access operators
     * \{
     */
    constexpr small_single_iterator & operator+=(difference_type n) noexcept(
      noexcept(std::is_nothrow_default_constructible_v<Value>))
    {
        if (n > 0)
            ++*this;
        else if (n < 0)
            --*this;
        return *this;
    }

    constexpr small_single_iterator & operator-=(difference_type n) noexcept(
      noexcept(std::is_nothrow_default_constructible_v<Value>))
    {
        operator+=(-n);
        return *this;
    }

    constexpr decltype(auto) operator[](difference_type n) const noexcept
    {
        auto tmp = *this;
        return *(tmp += n);
    }

    friend constexpr small_single_iterator operator+(small_single_iterator i, difference_type n) { return (i += n); }

    friend constexpr small_single_iterator operator+(difference_type n, small_single_iterator i) { return (i += n); }

    friend constexpr small_single_iterator operator-(small_single_iterator i, difference_type n) { return (i -= n); }

    friend constexpr difference_type operator-(small_single_iterator const & lhs, small_single_iterator const & rhs)
    {
        return static_cast<difference_type>((int)lhs.at_end - (int)rhs.at_end);
    }
    //!\}

    /*!\name Comparison operators
     * \{
     */
    constexpr friend bool operator==(small_single_iterator const & lhs, small_single_iterator const & rhs)
    {
        return lhs.at_end == rhs.at_end;
    }

    constexpr friend auto operator<=>(small_single_iterator const & lhs, small_single_iterator const & rhs)
    {
        return (int)lhs.at_end <=> (int)rhs.at_end;
    }
    //!\}
};

} // namespace radr::detail

namespace radr
{

//!\brief Determines how the value in radr::single_rng is stored.
enum class single_rng_storage
{
    indirect,   //!< Value storage is independent of range.
    in_range,   //!< Value is stored within range.
    in_iterator //!< Value is copied into every iterator.
};

/*!\brief A range of a single_rng value.
 * \tparam TVal The value type. Must be an object type (`const` allowed, reference not).
 * \tparam storage Storage behaviour of the value; see radr::single_rng_storage.
 *
 * \details
 *
 * |                       |  indirect    | in_range     | in_iterator    |
 * |-----------------------|--------------|--------------|----------------|
 * | category              | contiguous   | contiguous   | random_access  |
 * | borrowed_range        | yes          | no           | yes            |
 * | default-constructible | yes¹         | yes          | yes            |
 * | range_reference_t     | `TVal &`     | `TVal &`     | `TVal`         |
 * | constant_range        | no           | no           | yes            |
 *
 * All variants of this range are a `sized_range` and a `common_range` and `const_iterable`.
 *
 * ¹ Dereferecing the iterator of a default-constructed single_rng with indirect storage is undefined behaviour.
 */
template <std::copy_constructible TVal, single_rng_storage storage>
    requires std::is_object_v<TVal>
class single_rng : public rad_interface<single_rng<TVal, storage>>
{
    using TValNoConst = std::remove_const_t<TVal>;

    static_assert(storage == single_rng_storage::indirect || std::copy_constructible<TValNoConst>,
                  "The value type needs to be copy constructible.");

    static_assert(storage == single_rng_storage::indirect || std::default_initializable<TValNoConst>,
                  "The value type needs to be default-initializable.");

    static_assert(storage != single_rng_storage::in_iterator ||
                    (std::is_nothrow_default_constructible_v<TVal> && std::is_nothrow_copy_constructible_v<TVal>),
                  "The value type needs to be nothrow_default_constructible and nothrow_copy_constructible for "
                  "in_iterator storage.");

    using storage_type =
      std::conditional_t<storage == single_rng_storage::indirect, TVal *, detail::semiregular_box<TValNoConst>>;

    [[no_unique_address]] storage_type value{};

public:
    /*!\name Constructors: Rule-of-5
     * \{
     */
    constexpr single_rng()                               = default;
    constexpr single_rng(single_rng const &)             = default;
    constexpr single_rng(single_rng &&)                  = default;
    constexpr single_rng & operator=(single_rng const &) = default;
    constexpr single_rng & operator=(single_rng &&)      = default;
    //!\}

    /*!\name Constructors for indirect storage
     * \{
     */
    constexpr explicit single_rng(TVal & val)
        requires(storage == single_rng_storage::indirect)
      : value(&val)
    {}

    constexpr explicit single_rng(std::reference_wrapper<TVal> const & val)
        requires(storage == single_rng_storage::indirect)
      : value(&static_cast<TVal &>(val))
    {}

    constexpr explicit single_rng(TVal && val)
        requires(storage == single_rng_storage::indirect)
    = delete;
    //!\}

    /*!\name Constructors for in_range or in_iterator storage
     * \{
     */
    constexpr explicit single_rng(TValNoConst val)
        requires(storage != single_rng_storage::indirect)
      : value(std::in_place, std::move(val))
    {}

    template <class... _Args>
        requires std::constructible_from<TValNoConst, _Args...>
    constexpr explicit single_rng(std::in_place_t, _Args &&... args)
        requires(storage != single_rng_storage::indirect)
      : value{std::in_place, std::forward<_Args>(args)...}
    {}
    //!\}

    /*!\name Range access for in_iterator storage
     * \{
     */
    constexpr auto begin() const noexcept
        requires(storage == single_rng_storage::in_iterator)
    {
        return detail::small_single_iterator<TValNoConst>{*value};
    }

    constexpr auto end() const noexcept
        requires(storage == single_rng_storage::in_iterator)
    {
        return detail::small_single_iterator<TValNoConst>{};
    }
    //!\}

    /*!\name Range access for storage != in_iterator
     * \{
     */
    constexpr TVal * begin() noexcept
        requires(storage != single_rng_storage::in_iterator)
    {
        return data();
    }
    constexpr TVal const * begin() const noexcept
        requires(storage != single_rng_storage::in_iterator)
    {
        return data();
    }

    constexpr TVal * end() noexcept
        requires(storage != single_rng_storage::in_iterator)
    {
        return data() + 1;
    }
    constexpr TVal const * end() const noexcept
        requires(storage != single_rng_storage::in_iterator)
    {
        return data() + 1;
    }

    constexpr TVal * data() noexcept
        requires(storage == single_rng_storage::indirect)
    {
        return value;
    }
    constexpr TVal const * data() const noexcept
        requires(storage == single_rng_storage::indirect)
    {
        return value;
    }

    constexpr TVal * data() noexcept
        requires(storage == single_rng_storage::in_range)
    {
        return &*value;
    }
    constexpr TVal const * data() const noexcept
        requires(storage == single_rng_storage::in_range)
    {
        return &*value;
    }
    //!\}

    /*!\name Shared members
     * \{
     */
    static constexpr size_t size() noexcept { return 1; }

    constexpr friend bool operator==(single_rng const & lhs, single_rng const & rhs)
        requires detail::weakly_equality_comparable<TVal>
    {
        return lhs.front() == rhs.front();
    }
    //!\}
};

/*!\name Deduction guides
 * \{
 */
template <typename T>
single_rng(std::reference_wrapper<T> &&) -> single_rng<T, single_rng_storage::indirect>;
template <typename T>
single_rng(std::reference_wrapper<T> &) -> single_rng<T, single_rng_storage::indirect>;
template <typename T>
single_rng(std::reference_wrapper<T> const &) -> single_rng<T, single_rng_storage::indirect>;

template <typename T>
single_rng(T &&) -> single_rng<std::remove_reference_t<T>, single_rng_storage::in_range>;

template <typename T>
    requires(std::is_nothrow_default_constructible_v<T> && std::is_nothrow_copy_constructible_v<T> && sizeof(T) <= 64)
single_rng(T &&) -> single_rng<std::remove_cvref_t<T> const, single_rng_storage::in_iterator>;
//!\}

} // namespace radr

template <class T, radr::single_rng_storage storage>
inline constexpr bool std::ranges::enable_borrowed_range<radr::single_rng<T, storage>> =
  (storage != radr::single_rng_storage::in_range);
