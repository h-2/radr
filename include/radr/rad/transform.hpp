// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 The LLVM Project
// Copyright (c) 2023-2025 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <concepts>
#include <functional>
#include <iterator>
#include <ranges>

#include "../concepts.hpp"
#include "../detail/detail.hpp"
#include "../detail/pipe.hpp"
#include "../detail/semiregular_box.hpp"
#include "../generator.hpp"
#include "radr/range_access.hpp"

namespace radr::detail::transform
{

template <typename Invoc1, typename Invoc2>
struct nest_fn
{
    [[no_unique_address]] Invoc1 in1;
    [[no_unique_address]] Invoc2 in2;

    template <typename Val>
    constexpr decltype(auto) operator()(Val && val) const
      noexcept(noexcept(in1(std::forward<Val>(val))) && noexcept(in2(in1(std::forward<Val>(val)))))
    {
        return in2(in1(std::forward<Val>(val)));
    }
};

template <class Iter, class Fn>
concept fn_constraints = std::is_object_v<Fn> && std::copy_constructible<Fn> &&
                         std::regular_invocable<Fn const &, std::iter_reference_t<Iter>> &&
                         can_reference<std::invoke_result_t<Fn const &, std::iter_reference_t<Iter>>>;

} // namespace radr::detail::transform

namespace radr::detail
{

template <std::forward_iterator Iter, std::sentinel_for<Iter> Sent, typename Fn>
    requires detail::transform::fn_constraints<Iter, Fn>
class transform_sentinel;

template <std::forward_iterator Iter, typename Fn>
    requires detail::transform::fn_constraints<Iter, Fn>
class transform_iterator
{
    [[no_unique_address]] semiregular_box<Fn> func_;
    [[no_unique_address]] Iter                current_ = Iter();

    template <std::forward_iterator Iter_, typename Fn_>
        requires detail::transform::fn_constraints<Iter_, Fn_>
    friend class transform_iterator;
    template <std::forward_iterator Iter_, std::sentinel_for<Iter_> Sent_, typename Fn_>
        requires detail::transform::fn_constraints<Iter_, Fn_>
    friend class transform_sentinel;

    template <typename Container>
    constexpr friend auto tag_invoke(custom::rebind_iterator_tag,
                                     transform_iterator it,
                                     Container &        container_old,
                                     Container &        container_new)
    {
        it.current_ = tag_invoke(custom::rebind_iterator_tag{}, it.current_, container_old, container_new);
        return it;
    }

public:
    using iterator_concept =
      std::conditional_t<std::contiguous_iterator<Iter>, std::random_access_iterator_tag, iterator_tag_t<Iter>>;
    using iterator_category =
      std::conditional_t<std::is_reference_v<std::invoke_result_t<Fn const &, std::iter_reference_t<Iter>>>,
                         iterator_concept,
                         std::input_iterator_tag>;
    using value_type      = std::remove_cvref_t<std::invoke_result_t<Fn const &, std::iter_reference_t<Iter>>>;
    using difference_type = std::iter_difference_t<Iter>;

    transform_iterator() = default;

    constexpr transform_iterator(Fn func, Iter current) :
      func_(std::in_place, std::move(func)), current_(std::move(current))
    {}

    template <detail::different_from<Iter> OtherIter>
    constexpr transform_iterator(transform_iterator<OtherIter, Fn> i)
        requires std::convertible_to<OtherIter, Iter>
      : func_(std::move(i.func_)), current_(std::move(i.current_))
    {}

    constexpr Iter const & base() const & noexcept { return current_; }
    constexpr Iter         base() && { return std::move(current_); }

    constexpr Fn const & func() const & noexcept { return *func_; }
    constexpr Fn         func() && { return std::move(*func_); }

    constexpr decltype(auto) operator*() const noexcept(noexcept(std::invoke(*func_, *current_)))
    {
        return std::invoke(*func_, *current_);
    }

    constexpr transform_iterator & operator++()
    {
        ++current_;
        return *this;
    }

    constexpr void operator++(int) { ++current_; }

    constexpr transform_iterator operator++(int)
        requires std::forward_iterator<Iter>
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr transform_iterator & operator--()
        requires std::bidirectional_iterator<Iter>
    {
        --current_;
        return *this;
    }

    constexpr transform_iterator operator--(int)
        requires std::bidirectional_iterator<Iter>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr transform_iterator & operator+=(difference_type n)
        requires std::random_access_iterator<Iter>
    {
        current_ += n;
        return *this;
    }

    constexpr transform_iterator & operator-=(difference_type n)
        requires std::random_access_iterator<Iter>
    {
        current_ -= n;
        return *this;
    }

    constexpr decltype(auto) operator[](difference_type n) const noexcept(noexcept(std::invoke(*func_, current_[n])))
        requires std::random_access_iterator<Iter>
    {
        return std::invoke(*func_, current_[n]);
    }

    friend constexpr bool operator==(transform_iterator const & x, transform_iterator const & y)
        requires std::equality_comparable<Iter>
    {
        return x.current_ == y.current_;
    }

    friend constexpr bool operator<(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<Iter>
    {
        return x.current_ < y.current_;
    }

    friend constexpr bool operator>(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<Iter>
    {
        return x.current_ > y.current_;
    }

    friend constexpr bool operator<=(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<Iter>
    {
        return x.current_ <= y.current_;
    }

    friend constexpr bool operator>=(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<Iter>
    {
        return x.current_ >= y.current_;
    }

    friend constexpr auto operator<=>(transform_iterator const & x, transform_iterator const & y)
        requires std::random_access_iterator<Iter> && std::three_way_comparable<Iter>
    {
        return x.current_ <=> y.current_;
    }

    friend constexpr transform_iterator operator+(transform_iterator i, difference_type n)
        requires std::random_access_iterator<Iter>
    {
        return transform_iterator{*i.func_, i.current_ + n};
    }

    friend constexpr transform_iterator operator+(difference_type n, transform_iterator i)
        requires std::random_access_iterator<Iter>
    {
        return transform_iterator{*i.func_, i.current_ + n};
    }

    friend constexpr transform_iterator operator-(transform_iterator i, difference_type n)
        requires std::random_access_iterator<Iter>
    {
        return transform_iterator{*i.func_, i.current_ - n};
    }

    friend constexpr difference_type operator-(transform_iterator const & x, transform_iterator const & y)
        requires std::sized_sentinel_for<Iter, Iter>
    {
        return x.current_ - y.current_;
    }

    friend constexpr decltype(auto) iter_move(transform_iterator const & i) noexcept(noexcept(*i))
    {
        if constexpr (std::is_lvalue_reference_v<decltype(*i)>)
            return std::move(*i);
        else
            return *i;
    }
};

template <std::forward_iterator Iter, std::sentinel_for<Iter> Sen, typename Fn>
    requires detail::transform::fn_constraints<Iter, Fn>
class transform_sentinel
{
    [[no_unique_address]] Sen end_{};

    template <std::forward_iterator Iter_, std::sentinel_for<Iter_> Sent_, typename Fn_>
        requires detail::transform::fn_constraints<Iter_, Fn_>
    friend class transform_sentinel;

public:
    transform_sentinel() = default;

    constexpr explicit transform_sentinel(Sen end) : end_(end) {}

    constexpr transform_sentinel(Fn, Sen end) : end_(end) {}

    template <std::forward_iterator OtherIter, typename OtherSent>
    constexpr transform_sentinel(transform_sentinel<OtherIter, OtherSent, Fn> i)
        requires std::convertible_to<OtherSent, Sen> && std::convertible_to<OtherIter, Iter>
      : end_(std::move(i.end_))
    {}

    constexpr Sen base() const { return end_; }

    friend constexpr bool operator==(transform_iterator<Iter, Fn> const & x, transform_sentinel const & y)
    {
        // GCC<=12 doesn't handle  `.current_` here ¯\_(ツ)_/¯
        return x.base() == y.end_;
    }

    friend constexpr std::iter_difference_t<transform_iterator<Iter, Fn>> operator-(
      transform_iterator<Iter, Fn> const & x,
      transform_sentinel const &           y)
        requires std::sized_sentinel_for<Sen, Iter>
    {
        return x.current_ - y.end_;
    }

    friend constexpr std::iter_difference_t<transform_iterator<Iter, Fn>> operator-(
      transform_sentinel const &           x,
      transform_iterator<Iter, Fn> const & y)
        requires std::sized_sentinel_for<Sen, Iter>
    {
        return x.end_ - y.current_;
    }
};

inline constexpr auto transform_borrow_impl =
  []<typename UIt, typename USen, typename UCIt, typename UCSen, typename Fn, typename Size>(UIt  it,
                                                                                             USen sen,
                                                                                             UCIt,
                                                                                             UCSen,
                                                                                             Size size,
                                                                                             Fn   fn)
{
    using iterator_t = transform_iterator<UIt, Fn>;
    using sentinel_t = std::conditional_t<std::same_as<UIt, USen>, iterator_t, transform_sentinel<UIt, USen, Fn>>;

    using const_iterator_t = transform_iterator<UCIt, Fn>;
    using const_sentinel_t =
      std::conditional_t<std::same_as<UCIt, UCSen>, const_iterator_t, transform_sentinel<UCIt, UCSen, Fn>>;

    static constexpr auto kind = decays_to<Size, not_size> ? borrowing_rad_kind::unsized : borrowing_rad_kind::sized;

    using BorrowingRad = borrowing_rad<iterator_t, sentinel_t, const_iterator_t, const_sentinel_t, kind>;
    return BorrowingRad{
      iterator_t{fn,  it},
      sentinel_t{fn, sen},
      size
    };
};

inline constexpr auto transform_borrow =
  []<borrowed_mp_range URange, std::copy_constructible Fn>(URange && urange, Fn fn)
    requires(detail::transform::fn_constraints<radr::iterator_t<URange>, Fn>)
{
    // dispatch between generic case and chained case(s)
    return overloaded{
      transform_borrow_impl,
      []<typename UIt, typename UCIt, typename UFn, typename Size, typename Fn_>(transform_iterator<UIt, UFn>  iter,
                                                                                 transform_iterator<UIt, UFn>  sen,
                                                                                 transform_iterator<UCIt, UFn> citer,
                                                                                 transform_iterator<UCIt, UFn> csen,
                                                                                 Size                          size,
                                                                                 Fn_                           new_fn)
    {
        return transform_borrow_impl(std::move(iter).base(),
                                     std::move(sen).base(),
                                     std::move(citer).base(),
                                     std::move(csen).base(),
                                     size,
                                     transform::nest_fn{std::move(iter).func(), std::move(new_fn)});
    },
      []<typename UIt, typename USen, typename UCIt, typename UCSen, typename UFn, typename Size, typename Fn_>(
        transform_iterator<UIt, UFn>         iter,
        transform_sentinel<UIt, USen, UFn>   sen,
        transform_iterator<UCIt, UFn>        citer,
        transform_sentinel<UCIt, UCSen, UFn> csen,
        Size                                 size,
        Fn_                                  new_fn)
    {
        return transform_borrow_impl(std::move(iter).base(),
                                     std::move(sen).base(),
                                     std::move(citer).base(),
                                     std::move(csen).base(),
                                     size,
                                     transform::nest_fn{std::move(iter).func(), std::move(new_fn)});
    }}(radr::begin(urange),
       radr::end(urange),
       radr::cbegin(urange),
       radr::cend(urange),
       detail::size_or_not(urange),
       std::move(fn));
};

inline constexpr auto transform_coro =
  []<std::ranges::input_range URange, std::move_constructible Fn>(URange && urange, Fn fn)
    requires(std::invocable<Fn &, std::ranges::range_reference_t<URange>> &&
             can_reference<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_ASSERTSTRING_RVALUE);
    static_assert(std::movable<URange>, RADR_ASSERTSTRING_MOVABLE);

    // we need to create inner functor so that it can take by value
    return
      [](auto urange_, Fn fn) -> radr::generator<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>
    {
        for (auto && elem : urange_)
            co_yield fn(std::forward<decltype(elem)>(elem));
    }(std::move(urange), std::move(fn));
};

} // namespace radr::detail

namespace radr
{

inline namespace cpo
{
/*!\brief Transforms a range by applying an invocable on each element.
 * \param urange The underlying range.
 * \param[in] fn The invocable to apply
 *
 * ### Multi-pass adaptor
 *
 * * Requirements on \p urange : radr::mp_range
 * * Requirements on \p fn : std::copy_constructible, std::is_object_v, std::regular_invocable (`fn const &` with \p urange 's `reference_t` and `const_reference_t` )
 *
 * This adaptor preserves:
 *   * categories up to std::ranges::random_access_range
 *   * std::ranges::sized_range
 *   * std::ranges::borrowed_range
 *   * radr::common_range
 *   * radr::constant_range
 *   * radr::mutable_range (see below)
 *
 * Since transformers usually do not return references, mutability is lost anyway, and prefixing radr::as_const can
 * result in simpler types.
 *
 * Multiple nested transform adaptors are folded into one.
 *
 * ### Single-pass adaptor
 *
 * * Requirements on \p urange : std::ranges::input_range
 * * Requirements on \p fn : std::move_constructible, std::is_object_v, std::invocable (`fn &` with \p urange 's `reference_t`)
 *
 * The single-pass adaptor allows (observable) changes in the transformer (std::invocable instead of
 * std::regular_invocable).
 *
 */
inline constexpr auto transform = detail::pipe_with_args_fn{detail::transform_coro, detail::transform_borrow};
} // namespace cpo
} // namespace radr
