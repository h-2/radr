// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <functional>
#include <ranges>

#include "concepts.hpp"
#include "copyable_box.hpp"
#include "detail.hpp"
#include "generator.hpp"
#include "rad_interface.hpp"
#include "range_ref.hpp"

namespace radr::detail
{

template <class Fn, class URange>
concept regular_invocable_with_range_ref = std::regular_invocable<Fn, std::ranges::range_reference_t<URange>>;

}

namespace radr::detail::transform
{

template <class URange, class Fn>
concept constraints = rad_constraints<URange> && std::is_object_v<Fn> &&
                      std::regular_invocable<Fn &, std::ranges::range_reference_t<URange>> &&
                      detail::can_reference<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>;
}

namespace radr
{

template <std::ranges::input_range URange, std::copy_constructible Fn>
    requires detail::transform::constraints<URange, Fn>
class transform_rad : public rad_interface<transform_rad<URange, Fn>>
{
    template <bool>
    class iterator;
    template <bool>
    class sentinel;

    [[no_unique_address]] copyable_box<Fn> func_;
    [[no_unique_address]] URange          base_ = URange();

public:
    transform_rad()
        requires std::default_initializable<URange> && std::default_initializable<Fn>
    = default;

    constexpr explicit transform_rad(URange && base, Fn func) :
      func_(std::in_place, std::move(func)), base_(std::move(base))
    {}

    constexpr URange base() const &
        requires std::copy_constructible<URange>
    {
        return base_;
    }
    constexpr URange base() && { return std::move(base_); }

    constexpr iterator<false> begin() { return iterator<false>{*this, std::ranges::begin(base_)}; }
    constexpr iterator<true>  begin() const
        requires std::ranges::range<URange const> && detail::regular_invocable_with_range_ref<Fn const &, URange const>
    {
        return iterator<true>(*this, std::ranges::begin(base_));
    }

    constexpr sentinel<false> end() { return sentinel<false>(std::ranges::end(base_)); }
    constexpr iterator<false> end()
        requires std::ranges::common_range<URange>
    {
        return iterator<false>(*this, std::ranges::end(base_));
    }
    constexpr sentinel<true> end() const
        requires std::ranges::range<URange const> && detail::regular_invocable_with_range_ref<Fn const &, URange const>
    {
        return sentinel<true>(std::ranges::end(base_));
    }
    constexpr iterator<true> end() const
        requires std::ranges::common_range<URange const> &&
                 detail::regular_invocable_with_range_ref<Fn const &, URange const>
    {
        return iterator<true>(*this, std::ranges::end(base_));
    }

    constexpr auto size()
        requires std::ranges::sized_range<URange>
    {
        return std::ranges::size(base_);
    }
    constexpr auto size() const
        requires std::ranges::sized_range<URange const>
    {
        return std::ranges::size(base_);
    }
};

template <class Range, class Fn>
transform_rad(Range &&, Fn) -> transform_rad<std::remove_cvref_t<Range>, Fn>;

} // namespace radr

namespace radr::detail::transform
{

template <class URange>
struct iterator_concept
{
    using type = std::input_iterator_tag;
};

template <std::ranges::random_access_range URange>
struct iterator_concept<URange>
{
    using type = std::random_access_iterator_tag;
};

template <std::ranges::bidirectional_range URange>
struct iterator_concept<URange>
{
    using type = std::bidirectional_iterator_tag;
};

template <std::ranges::forward_range URange>
struct iterator_concept<URange>
{
    using type = std::forward_iterator_tag;
};

template <class, class>
struct iterator_category_base
{};

template <std::ranges::forward_range URange, class Fn>
struct iterator_category_base<URange, Fn>
{
    using Cat = typename std::iterator_traits<std::ranges::iterator_t<URange>>::iterator_category;

    using iterator_category = std::conditional_t<
      std::is_reference_v<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>,
      std::conditional_t<std::derived_from<Cat, std::contiguous_iterator_tag>, std::random_access_iterator_tag, Cat>,
      std::input_iterator_tag>;
};

} // namespace radr::detail::transform

namespace radr
{

template <std::ranges::input_range URange, std::copy_constructible Fn>
    requires detail::transform::constraints<URange, Fn>
template <bool Const>
class transform_rad<URange, Fn>::iterator : public detail::transform::iterator_category_base<URange, Fn>
{
    using Parent = detail::maybe_const<Const, transform_rad>;
    using Base   = detail::maybe_const<Const, URange>;

    Parent * parent_ = nullptr;

    template <bool>
    friend class transform_rad<URange, Fn>::iterator;

    template <bool>
    friend class transform_rad<URange, Fn>::sentinel;

public:
    std::ranges::iterator_t<Base> current_ = std::ranges::iterator_t<Base>();

    using iterator_concept = typename detail::transform::iterator_concept<URange>::type;
    using value_type       = std::remove_cvref_t<std::invoke_result_t<Fn &, std::ranges::range_reference_t<Base>>>;
    using difference_type  = std::ranges::range_difference_t<Base>;

    iterator()
        requires std::default_initializable<std::ranges::iterator_t<Base>>
    = default;

    constexpr iterator(Parent & parent, std::ranges::iterator_t<Base> current) :
      parent_(std::addressof(parent)), current_(std::move(current))
    {}

    // Note: `i` should always be `iterator<false>`, but directly using
    // `iterator<false>` is ill-formed when `Const` is false
    // (see http://wg21.link/class.copy.ctor#5).
    constexpr iterator(iterator<!Const> i)
        requires Const && std::convertible_to<std::ranges::iterator_t<URange>, std::ranges::iterator_t<Base>>
      : parent_(i.parent_), current_(std::move(i.current_))
    {}

    constexpr std::ranges::iterator_t<Base> const & base() const & noexcept { return current_; }

    constexpr std::ranges::iterator_t<Base> base() && { return std::move(current_); }

    constexpr decltype(auto) operator*() const noexcept(noexcept(std::invoke(*parent_->func_, *current_)))
    {
        return std::invoke(*parent_->func_, *current_);
    }

    constexpr iterator & operator++()
    {
        ++current_;
        return *this;
    }

    constexpr void operator++(int) { ++current_; }

    constexpr iterator operator++(int)
        requires std::ranges::forward_range<Base>
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    constexpr iterator & operator--()
        requires std::ranges::bidirectional_range<Base>
    {
        --current_;
        return *this;
    }

    constexpr iterator operator--(int)
        requires std::ranges::bidirectional_range<Base>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    constexpr iterator & operator+=(difference_type n)
        requires std::ranges::random_access_range<Base>
    {
        current_ += n;
        return *this;
    }

    constexpr iterator & operator-=(difference_type n)
        requires std::ranges::random_access_range<Base>
    {
        current_ -= n;
        return *this;
    }

    constexpr decltype(auto) operator[](difference_type n) const
      noexcept(noexcept(std::invoke(*parent_->func_, current_[n])))
        requires std::ranges::random_access_range<Base>
    {
        return std::invoke(*parent_->func_, current_[n]);
    }

    friend constexpr bool operator==(iterator const & x, iterator const & y)
        requires std::equality_comparable<std::ranges::iterator_t<Base>>
    {
        return x.current_ == y.current_;
    }

    friend constexpr bool operator<(iterator const & x, iterator const & y)
        requires std::ranges::random_access_range<Base>
    {
        return x.current_ < y.current_;
    }

    friend constexpr bool operator>(iterator const & x, iterator const & y)
        requires std::ranges::random_access_range<Base>
    {
        return x.current_ > y.current_;
    }

    friend constexpr bool operator<=(iterator const & x, iterator const & y)
        requires std::ranges::random_access_range<Base>
    {
        return x.current_ <= y.current_;
    }

    friend constexpr bool operator>=(iterator const & x, iterator const & y)
        requires std::ranges::random_access_range<Base>
    {
        return x.current_ >= y.current_;
    }

    friend constexpr auto operator<=>(iterator const & x, iterator const & y)
        requires std::ranges::random_access_range<Base> && std::three_way_comparable<std::ranges::iterator_t<Base>>
    {
        return x.current_ <=> y.current_;
    }

    friend constexpr iterator operator+(iterator i, difference_type n)
        requires std::ranges::random_access_range<Base>
    {
        return iterator{*i.parent_, i.current_ + n};
    }

    friend constexpr iterator operator+(difference_type n, iterator i)
        requires std::ranges::random_access_range<Base>
    {
        return iterator{*i.parent_, i.current_ + n};
    }

    friend constexpr iterator operator-(iterator i, difference_type n)
        requires std::ranges::random_access_range<Base>
    {
        return iterator{*i.parent_, i.current_ - n};
    }

    friend constexpr difference_type operator-(iterator const & x, iterator const & y)
        requires std::sized_sentinel_for<std::ranges::iterator_t<Base>, std::ranges::iterator_t<Base>>
    {
        return x.current_ - y.current_;
    }

    friend constexpr decltype(auto) iter_move(iterator const & i) noexcept(noexcept(*i))
    {
        if constexpr (std::is_lvalue_reference_v<decltype(*i)>)
            return std::move(*i);
        else
            return *i;
    }
};

template <std::ranges::input_range URange, std::copy_constructible Fn>
    requires detail::transform::constraints<URange, Fn>
template <bool Const>
class transform_rad<URange, Fn>::sentinel
{
    using Parent = detail::maybe_const<Const, transform_rad>;
    using Base   = detail::maybe_const<Const, URange>;

    std::ranges::sentinel_t<Base> end_ = std::ranges::sentinel_t<Base>();

    template <bool>
    friend class transform_rad<URange, Fn>::iterator;

    template <bool>
    friend class transform_rad<URange, Fn>::sentinel;

public:
    sentinel() = default;

    constexpr explicit sentinel(std::ranges::sentinel_t<Base> end) : end_(end) {}

    // Note: `i` should always be `sentinel<false>`, but directly using
    // `sentinel<false>` is ill-formed when `Const` is false
    // (see http://wg21.link/class.copy.ctor#5).
    constexpr sentinel(sentinel<!Const> i)
        requires Const && std::convertible_to<std::ranges::sentinel_t<URange>, std::ranges::sentinel_t<Base>>
      : end_(std::move(i.end_))
    {}

    constexpr std::ranges::sentinel_t<Base> base() const { return end_; }

    template <bool OtherConst>
        requires std::sentinel_for<std::ranges::sentinel_t<Base>,
                                   std::ranges::iterator_t<detail::maybe_const<OtherConst, URange>>>
    friend constexpr bool operator==(iterator<OtherConst> const & x, sentinel const & y)
    {
        return x.current_ == y.end_;
    }

    template <bool OtherConst>
        requires std::sized_sentinel_for<std::ranges::sentinel_t<Base>,
                                         std::ranges::iterator_t<detail::maybe_const<OtherConst, URange>>>
    friend constexpr std::ranges::range_difference_t<detail::maybe_const<OtherConst, URange>> operator-(
      iterator<OtherConst> const & x,
      sentinel const &             y)
    {
        return x.current_ - y.end_;
    }

    template <bool OtherConst>
        requires std::sized_sentinel_for<std::ranges::sentinel_t<Base>,
                                         std::ranges::iterator_t<detail::maybe_const<OtherConst, URange>>>
    friend constexpr std::ranges::range_difference_t<detail::maybe_const<OtherConst, URange>> operator-(
      sentinel const &             x,
      iterator<OtherConst> const & y)
    {
        return x.end_ - y.current_;
    }
};

inline constexpr auto transform_coro = []<adaptable_range URange, std::copy_constructible Fn>(URange && urange, Fn fn)
    requires(std::regular_invocable<Fn &, std::ranges::range_reference_t<URange>> &&
             detail::can_reference<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>)
{
    static_assert(!std::is_lvalue_reference_v<URange>, RADR_RVALUE_ASSERTION_STRING);

    // we need to create inner functor so that it can take by value
    return
      [](auto urange_, Fn fn) -> radr::generator<std::invoke_result_t<Fn &, std::ranges::range_reference_t<URange>>>
    {
        for (auto && elem : urange_)
            co_yield fn(std::forward<decltype(elem)>(elem));
    }(std::move(urange), std::move(fn));
};

} // namespace radr

namespace radr::detail
{

struct transform_fn
{
    template <std::ranges::input_range Range, class Fn>
    [[nodiscard]] constexpr auto operator()(Range && range, Fn && f) const
      noexcept(noexcept(transform_coro(std::forward<Range>(range), std::forward<Fn>(f))))
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_RVALUE_ASSERTION_STRING);
        return transform_coro(std::forward<Range>(range), std::forward<Fn>(f));
    }

    template <std::ranges::forward_range Range, class Fn>
    [[nodiscard]] constexpr auto operator()(Range && range, Fn && f) const
      noexcept(noexcept(transform_rad(std::forward<Range>(range), std::forward<Fn>(f))))
    {
        static_assert(!std::is_lvalue_reference_v<Range>, RADR_RVALUE_ASSERTION_STRING);
        return transform_rad(std::forward<Range>(range), std::forward<Fn>(f));
    }

    template <class Range, class Fn>
    [[nodiscard]] constexpr auto operator()(std::reference_wrapper<Range> const & range, Fn && f) const
      noexcept(noexcept(operator()(range_ref{range}, std::forward<Fn>(f))))
        -> decltype(operator()(range_ref{range}, std::forward<Fn>(f)))
    {
        return operator()(range_ref{range}, std::forward<Fn>(f));
    }

    template <class Fn>
        requires std::constructible_from<std::decay_t<Fn>, Fn>
    [[nodiscard]] constexpr auto operator()(Fn && f) const
      noexcept(std::is_nothrow_constructible_v<std::decay_t<Fn>, Fn>)
    {
        return range_adaptor_closure_t{detail::bind_back(*this, std::forward<Fn>(f))};
    }
};

} // namespace radr::detail

namespace radr::pipe
{

inline namespace cpo
{
inline constexpr auto transform = detail::transform_fn{};
} // namespace cpo
} // namespace radr::pipe

template <class Range, class Fn>
inline constexpr bool std::ranges::enable_view<radr::transform_rad<Range, Fn>> = std::ranges::view<Range>;
