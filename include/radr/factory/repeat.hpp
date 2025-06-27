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
#include <cstddef>
#include <iterator>
#include <type_traits>

#include "radr/custom/tags.hpp"
#include "radr/detail/detail.hpp"
#include "radr/detail/semiregular_box.hpp"
#include "radr/rad_util/rad_interface.hpp"

namespace radr
{

//!\brief Determines how the value in radr::repeat_rng is stored.
enum class repeat_rng_storage
{
    indirect,   //!< Value storage is independent of range.
    in_range,   //!< Value is stored within range.
    in_iterator //!< Value is copied into every iterator.
};

} // namespace radr

namespace radr::detail
{

template <typename T>
concept IntegralConstant = requires { std::integral_constant<ptrdiff_t, T::value>::value; };

template <typename T>
concept bound_reqs = one_of<T, ptrdiff_t, std::unreachable_sentinel_t> || IntegralConstant<T>;

template <typename T>
using deduce_bound_t = std::conditional_t<std::integral<std::remove_cvref_t<T>>, ptrdiff_t, std::remove_cvref_t<T>>;

//!\brief We want to deduce the the stored value type from `T &` to `T`, `T const &` to `T const`, and `T` to `T const`.
template <typename T, typename Ref = std::iter_reference_t<T>>
using tval_from_iterator = std::conditional_t<std::is_reference_v<Ref>, std::remove_reference_t<Ref>, Ref const>;

template <typename Value, repeat_rng_storage storage>
    requires std::is_object_v<Value>
class repeat_iterator
{
    using TValNoConst = std::remove_const_t<Value>;
    static_assert(storage == repeat_rng_storage::in_iterator || (std::is_nothrow_default_constructible_v<TValNoConst> &&
                                                                 std::is_nothrow_copy_constructible_v<TValNoConst>),
                  "The value type most be nothrow copy and default constructible.");

    using storage_type = std::conditional_t<storage == repeat_rng_storage::in_iterator, TValNoConst, Value *>;

    [[no_unique_address]] storage_type value{};
    [[no_unique_address]] ptrdiff_t    current_{};

    template <typename Value2, repeat_rng_storage storage2>
        requires std::is_object_v<Value2>
    friend class repeat_iterator;

public:
    using iterator_category = std::conditional_t<storage == repeat_rng_storage::in_iterator,
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

    constexpr repeat_iterator(Value * value_, ptrdiff_t bound_)
        requires(storage == repeat_rng_storage::in_iterator)
      : value{*value_}, current_{bound_}
    {}

    constexpr repeat_iterator(Value * value_, ptrdiff_t bound_) : value{value_}, current_{bound_} {}

    //!\brief Non-const iterator to const_iterator.
    template <std::same_as<TValNoConst> OtherVal>
        requires(!std::same_as<TValNoConst, Value>)
    constexpr repeat_iterator(repeat_iterator<OtherVal, storage> it) : value{it.value}, current_{it.current_}
    {}
    //!\}

    /*!\name Forward operators
     * \{
     */

    constexpr Value & operator*() const noexcept { return *value; }

    constexpr TValNoConst operator*() const noexcept
        requires(storage == repeat_rng_storage::in_iterator)
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
template <typename Value, radr::repeat_rng_storage storage>
struct std::iterator_traits<radr::detail::repeat_iterator<Value, storage>>
{
    using difference_type   = ptrdiff_t;
    using value_type        = std::remove_const_t<Value>;
    using pointer           = void;
    using reference         = std::conditional_t<storage == radr::repeat_rng_storage::in_iterator, value_type, Value *>;
    using iterator_category = std::random_access_iterator_tag;
};
#endif

namespace radr
{

//!\brief Compile-time constant.
template <auto v>
inline constexpr std::integral_constant<decltype(v), v> constant{};

template <auto v>
using constant_t = std::integral_constant<decltype(v), v>;

/*!\brief A range of a (possibly infinitely) repeated value.
 * \tparam TVal The value type. Must be an object type (`const` allowed, reference not).
 * \tparam Bound Either `ptrdiff_t`, randr::constant_t or std::unreachable_sentinel_t.
 * \tparam storage Storage behaviour of the value; see radr::repeat_rng_storage.
 *
 * \details
 *
 * A range that repeats an element N times, where N can be 0 to infinity. The template can be parametrised in two
 * dimensions: \p Bound and \p storage.
 *
 * All instantiations of `repeat_rng` model at least std::semiregular, std::ranges::random_access_range and
 * radr::mp_range.
 *
 * ## Bound
 *
 * The bound must be one of the following three:
 *
 *  1. `ptrdiff_t` (dynamic/run-time bound)
 *  2. `std::integral_constant<ptrdiff_t, X>` (static/compile-time bound of value X)
 *  3. `std::unreachable_sentinel_t` (unbounded/infinite)
 *
 * The first two options lead to repeat_rng modelling std::ranges::sized_range;
 * the third does not.
 *
 * ## Storage
 *
 * The storage parameter indicates where the value is stored that the range produces.
 * It is specified via radr::repeat_rng_storage; it must be one of:
 *
 *   1. radr::repeat_rng_storage::indirect (external storage)
 *   2. radr::repeat_rng_storage::in_range (stored within range)
 *   3. radr::repeat_rng_storage::in_iterator (stored within iterator)
 *
 * |                       |  indirect      | in_range       | in_iterator    |
 * |-----------------------|----------------|----------------|----------------|
 * | `borrowed_range`      | yes            | no             | yes            |
 * | range_reference_t     | `TVal &`       | `TVal &`       | `TVal`         |
 *
 * The above table shows some implications of the storage choice.
 *
 * ## Interactions between Bound and Storage
 *
 * ### Default construction
 *
 * |  Bound \ storage                         |  indirect      | in_range        | in_iterator     |
 * |------------------------------------------|:--------------:|:---------------:|:---------------:|
 * | `ptrdiff_t` (dynamic)                    | empty          | empty           | empty           |
 * | `std::integral_constant<ptrdiff_t, 0>`   | empty          | empty           | empty           |
 * | `std::integral_constant<ptrdiff_t, 1>`   | ⚠ UB ⚠          | defaulted elem  | defaulted elem  |
 * | `std::integral_constant<ptrdiff_t, X>`   | ⚠ UB ⚠          | defaulted elem  | defaulted elem  |
 * | `std::unreachable_sentinel_t` (infinite) | ⚠ UB ⚠          | defaulted elem  | defaulted elem  |
 *
 * `repeat_rng` is always default-initializable. The above table shows that some default-initialised instantiations
 * are constructed with a default-initialised element. This is safe to read. Other instantiations are empty.
 *
 * But some are also in a non-empty "detached" state. Reading from these would result in nullptr read (precondition
 * violation / undefined behaviour).
 *
 * ### Range category
 *
 * |  Bound \ storage                       |  indirect      | in_range       | in_iterator    |
 * |----------------------------------------|:--------------:|:--------------:|:--------------:|
 * | `ptrdiff_t`                            | random_access  | random_access  | random_access  |
 * | `std::integral_constant<ptrdiff_t, 0>` | contiguous     | contiguous     | random_access  |
 * | `std::integral_constant<ptrdiff_t, 1>` | contiguous     | contiguous     | random_access  |
 * | `std::integral_constant<ptrdiff_t, X>` | random_access  | random_access  | random_access  |
 * | `std::unreachable_sentinel_t`          | random_access  | random_access  | random_access  |
 *
 * All instantiations model std::ranges::random_access_range. Instantiations of static size 0 or 1 that do not store
 * in-iterator, also model std::ranges::contiguous_range.
 */
template <typename TVal, typename Bound, repeat_rng_storage storage>
    requires(std::is_object_v<TVal> && detail::bound_reqs<Bound>)
class repeat_rng : public rad_interface<repeat_rng<TVal, Bound, storage>>
{
    using TValNoConst = std::remove_const_t<TVal>;

    struct empty_t
    {
        constexpr empty_t() noexcept                            = default;
        constexpr empty_t(empty_t const &) noexcept             = default;
        constexpr empty_t(empty_t &&) noexcept                  = default;
        constexpr empty_t & operator=(empty_t const &) noexcept = default;
        constexpr empty_t & operator=(empty_t &&) noexcept      = default;

        constexpr empty_t(auto) noexcept {}
    };

    static_assert(detail::bound_reqs<Bound>,
                  "The bound must be one of:\n"
                  " ptrdiff_t                               (dynamic)\n"
                  " std::integral_constant<ptrdiff_t, X>    (static)\n"
                  " std::unreachable_sentinel_t             (infinite)");

    static_assert(storage == repeat_rng_storage::indirect || std::copy_constructible<TValNoConst>,
                  "The value type needs to be copy constructible.");

    static_assert(storage == repeat_rng_storage::indirect || std::default_initializable<TValNoConst>,
                  "The value type needs to be default-initializable.");

    static_assert(storage != repeat_rng_storage::in_iterator || (std::is_nothrow_default_constructible_v<TValNoConst> &&
                                                                 std::is_nothrow_copy_constructible_v<TValNoConst>),
                  "The value type needs to be nothrow_default_constructible and nothrow_copy_constructible for "
                  "in_iterator storage.");

    static consteval ptrdiff_t static_size()
    {
        if constexpr (detail::IntegralConstant<Bound>)
        {
            static_assert(Bound::value >= 0, "Static bound must be >= 0");
            return Bound::value;
        }
        else
        {
            return -1;
        }
    }

    //!\brief The constant bound is not passed to the iterator.
    using IteratorBound =
      std::conditional_t<static_size() == 0, empty_t, std::conditional_t<static_size() >= 1, ptrdiff_t, Bound>>;

    using storage_type = std::conditional_t<
      static_size() == 0,
      empty_t,
      std::conditional_t<storage == repeat_rng_storage::indirect, TVal *, detail::semiregular_box<TValNoConst>>>;

    [[no_unique_address]] storage_type  value{};
    [[no_unique_address]] IteratorBound bound_ = Bound();

    TVal * value_addr()
    {
        if constexpr (static_size() == 0)
            return nullptr;
        else if constexpr (storage == repeat_rng_storage::indirect)
            return value;
        else // unpeal box
            return &*value;
    }

    TVal const * value_addr() const
    {
        if constexpr (static_size() == 0)
            return nullptr;
        else if constexpr (storage == repeat_rng_storage::indirect)
            return value;
        else // unpeal box
            return &*value;
    }

    static constexpr bool const_symmetric = std::is_const_v<TVal> || storage == repeat_rng_storage::in_iterator;

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
        requires(storage == repeat_rng_storage::indirect)
      : value(&val), bound_(std::move(b))
    {}

    constexpr explicit repeat_rng(std::reference_wrapper<TVal> const & val, Bound b = Bound())
        requires(storage == repeat_rng_storage::indirect)
      : value(&static_cast<TVal &>(val)), bound_(std::move(b))
    {}

    constexpr explicit repeat_rng(TVal && val, Bound b = Bound())
        requires(storage == repeat_rng_storage::indirect)
    = delete;
    //!\}

    /*!\name Constructors for in_range or in_iterator storage
     * \{
     */
    constexpr explicit repeat_rng(TVal val, Bound b = Bound())
        requires(storage != repeat_rng_storage::indirect)
      : value(std::in_place, std::move(val)), bound_(std::move(b))
    {}

    template <class... TArgs, class... BoundArgs>
        requires std::constructible_from<TValNoConst, TArgs...> && std::constructible_from<Bound, BoundArgs...> &&
                   (storage != repeat_rng_storage::indirect)
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
        if constexpr (static_size() == 0 || (static_size() == 1 && storage != repeat_rng_storage::in_iterator))
            return value_addr();
        else
            return detail::repeat_iterator<TVal, storage>{value_addr(), 0};
    }

    constexpr auto begin() const noexcept
    {
        if constexpr (static_size() == 0 || (static_size() == 1 && storage != repeat_rng_storage::in_iterator))
            return value_addr();
        else
            return detail::repeat_iterator<TVal const, storage>{value_addr(), 0};
    }

    constexpr auto end() noexcept
        requires(!std::same_as<Bound, std::unreachable_sentinel_t> && !const_symmetric)
    {
        if constexpr (static_size() == 0)
            return value_addr();
        else if constexpr (static_size() == 1 && storage != repeat_rng_storage::in_iterator)
            return value_addr() + 1;
        else
            return detail::repeat_iterator<TVal, storage>{value_addr(), bound_};
    }

    constexpr auto end() const noexcept
        requires(!std::same_as<Bound, std::unreachable_sentinel_t>)
    {
        if constexpr (static_size() == 0)
            return value_addr();
        else if constexpr (static_size() == 1 && storage != repeat_rng_storage::in_iterator)
            return value_addr() + 1;
        else
            return detail::repeat_iterator<TVal const, storage>{value_addr(), bound_};
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

    static constexpr bool empty() noexcept
        requires detail::IntegralConstant<Bound>
    {
        return Bound::value == 0;
    }

    using rad_interface<repeat_rng<TVal, Bound, storage>>::empty;

    static constexpr size_t size() noexcept
        requires detail::IntegralConstant<Bound>
    {
        return Bound::value;
    }

    using rad_interface<repeat_rng<TVal, Bound, storage>>::size;

    /*!\brief Compares the value and the size.
     * \pre Neither side is an unbounded **and** indirect repeat_rng that is default-initialised.
     *
     * ## Complexity
     *
     * Constant.
     */
    constexpr friend bool operator==(repeat_rng const & lhs, repeat_rng const & rhs)
        requires detail::weakly_equality_comparable<TVal>
    {
        if constexpr (std::same_as<Bound, ptrdiff_t>)
            return lhs.bound_ == rhs.bound_ && lhs.bound_ > 0 && lhs.front() == rhs.front();
        else if constexpr (static_size() == 0)
            return true;
        else // non-empty static bound or std::unreachable_sentinel_t
            return lhs.front() == rhs.front();
    }
    //!\}

    /*!\name Create custom subranges.
     * \details
     *
     * Borrowed repeat ranges (those without in-range storage) have special subrange overloads:
     *
     *   * Ranges of static size 0 always lead to ranges of static size 0.
     *   * A subrange with std::unreachable_sentinel as sentinel, leads to an infinite range.
     *   * All subranges of two iterators result in a dynamically-sized, bounded repeat_rng (indepent
     * of the original range's bound).
     *   * Storage behaviour is always preserved.
     *
     * \{
     */
    //!\brief Generic (it, sen) subborrow.
    template <std::convertible_to<repeat_rng> R>
        requires(storage != repeat_rng_storage::in_range)
    constexpr friend auto tag_invoke(custom::subborrow_tag, R &&, iterator_t<R> it, iterator_t<R> sen)
    {
        if constexpr (static_size() == 0)
            return repeat_rng<detail::tval_from_iterator<iterator_t<R>>, constant_t<0>, storage>{};
        else if constexpr (static_size() > 0)
            return repeat_rng<detail::tval_from_iterator<iterator_t<R>>, ptrdiff_t, storage>{*it,
                                                                                             std::min(sen - it,
                                                                                                      static_size())};
        else
            return repeat_rng<detail::tval_from_iterator<iterator_t<R>>, ptrdiff_t, storage>{*it, sen - it};
    }

    //!\brief Generic (it, unreachable) subborrow.
    template <std::convertible_to<repeat_rng> R>
        requires(storage != repeat_rng_storage::in_range)
    constexpr friend auto tag_invoke(custom::subborrow_tag, R &&, iterator_t<R> it, std::unreachable_sentinel_t)
    {
        return repeat_rng<detail::tval_from_iterator<iterator_t<R>>, std::unreachable_sentinel_t, storage>{*it};
    }

    //!\brief Generic (it, ?, size) subborrow.
    template <detail::decays_to<repeat_rng> R, typename Sen>
        requires(storage != repeat_rng_storage::in_range)
    constexpr friend auto tag_invoke(custom::subborrow_tag, R &&, iterator_t<R> it, Sen, size_t s)
    {
        if constexpr (static_size() == 0)
            return repeat_rng<detail::tval_from_iterator<iterator_t<R>>, constant_t<0>, storage>{};
        else if constexpr (static_size() > 0)
            return repeat_rng<detail::tval_from_iterator<iterator_t<R>>, ptrdiff_t, storage>{
              *it,
              std::min(static_cast<ptrdiff_t>(s), static_size())};
        else
            return repeat_rng<detail::tval_from_iterator<iterator_t<R>>, ptrdiff_t, storage>{*it,
                                                                                             static_cast<ptrdiff_t>(s)};
    }
    //!\}
};

/*!\name Deduction guides
 * \{
 */
//!\brief Default is in_range storage.
template <typename T>
repeat_rng(T &&) -> repeat_rng<std::remove_reference_t<T>, std::unreachable_sentinel_t, repeat_rng_storage::in_range>;
//!\brief Default is in_range storage.
template <typename T, typename Bounds>
repeat_rng(T &&, Bounds)
  -> repeat_rng<std::remove_reference_t<T>, detail::deduce_bound_t<Bounds>, repeat_rng_storage::in_range>;

//!\brief Small types default to in_iterator storage.
template <typename T>
    requires small_type<std::remove_cvref_t<T>>
repeat_rng(T &&)
  -> repeat_rng<std::remove_reference_t<T> const, std::unreachable_sentinel_t, repeat_rng_storage::in_iterator>;
//!\brief Small types default to in_iterator storage.
template <typename T, typename Bounds>
    requires small_type<std::remove_cvref_t<T>>
repeat_rng(T &&, Bounds)
  -> repeat_rng<std::remove_reference_t<T> const, detail::deduce_bound_t<Bounds>, repeat_rng_storage::in_iterator>;

//!\brief Reference wrapper lead to indirect storage.
template <typename T>
repeat_rng(std::reference_wrapper<T> &&) -> repeat_rng<T, std::unreachable_sentinel_t, repeat_rng_storage::indirect>;
//!\brief Reference wrapper lead to indirect storage.
template <typename T>
repeat_rng(std::reference_wrapper<T> &) -> repeat_rng<T, std::unreachable_sentinel_t, repeat_rng_storage::indirect>;
//!\brief Reference wrapper lead to indirect storage.
template <typename T>
repeat_rng(std::reference_wrapper<T> const &)
  -> repeat_rng<T, std::unreachable_sentinel_t, repeat_rng_storage::indirect>;
//!\brief Reference wrapper lead to indirect storage.
template <typename T, typename Bounds>
repeat_rng(std::reference_wrapper<T> &&, Bounds)
  -> repeat_rng<T, detail::deduce_bound_t<Bounds>, repeat_rng_storage::indirect>;
//!\brief Reference wrapper lead to indirect storage.
template <typename T, typename Bounds>
repeat_rng(std::reference_wrapper<T> &, Bounds)
  -> repeat_rng<T, detail::deduce_bound_t<Bounds>, repeat_rng_storage::indirect>;
//!\brief Reference wrapper lead to indirect storage.
template <typename T, typename Bounds>
repeat_rng(std::reference_wrapper<T> const &, Bounds)
  -> repeat_rng<T, detail::deduce_bound_t<Bounds>, repeat_rng_storage::indirect>;
//!\}

} // namespace radr

namespace radr::detail
{

struct repeat_t
{
    template <typename TVal>
    constexpr auto operator()(TVal && val) const
    {
        return repeat_rng{std::forward<TVal>(val)};
    }

    template <typename TVal, typename Bound>
    constexpr auto operator()(TVal && val, Bound && bound) const
        requires requires {
            repeat_rng{std::forward<TVal>(val), std::forward<Bound>(bound)};
        }
    {
        return repeat_rng{std::forward<TVal>(val), std::forward<Bound>(bound)};
    }
};

} // namespace radr::detail

namespace radr
{
/*!\brief A range factory that produces a range which repeats a specific element 0-N (possibly infinite) times.
 * \param value The value.
 * \param bound The bound; optional; defaults to std::unreachable_sentinel.
 *
 * \details
 *
 * This factory returns an object of type radr::repeat_rng. See the respective documentation for details.
 *
 * In particular, note that for the value parameter:
 *
 *   * By default, the value passed will be stored in the range.
 *   * Small values will also be copied into the iterator.
 *   * Values wrapped in `std::ref()` will be referenced (not stored).
 *
 * For the bound parameter:
 *   * An integral value will lead to a dynamic / run-time bound of that value.
 *   * `radr::constant<X>` will lead to a static / compile-time bound of X.
 *   * std::unreachable_sentinel will lead to an unbounded / infinite range. This is the default.
 */
inline constexpr detail::repeat_t repeat{};

} // namespace radr

template <class T, typename Bound, radr::repeat_rng_storage storage>
inline constexpr bool std::ranges::enable_borrowed_range<radr::repeat_rng<T, Bound, storage>> =
  (storage != radr::repeat_rng_storage::in_range);
