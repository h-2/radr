// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2024 Microsoft
// Copyright (c) 2024 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../concepts.hpp"

namespace radr::detail
{

template <std::input_iterator Iter>
class basic_const_iterator;

}

namespace radr
{

template <std::input_iterator Iter>
using const_iterator = std::conditional_t<constant_iterator<Iter>, Iter, detail::basic_const_iterator<Iter>>;

}

namespace radr::detail
{

template <class Sent>
struct const_sentinel_impl
{
    using type = Sent;
};

template <std::input_iterator Sent>
struct const_sentinel_impl<Sent>
{
    using type = const_iterator<Sent>;
};

} // namespace radr::detail

namespace radr
{

template <std::semiregular Sent>
using const_sentinel = detail::const_sentinel_impl<Sent>::type;

}

namespace radr::detail
{

template <std::indirectly_readable Iter>
using Iter_const_rvalue_reference_t =
  std::common_reference_t<std::iter_value_t<Iter> const &&, std::iter_rvalue_reference_t<Iter>>;

template <typename T>
struct not_a_const_iterator_impl : std::true_type
{};

template <std::input_iterator Iter>
struct not_a_const_iterator_impl<basic_const_iterator<Iter>> : std::false_type
{};

template <class Ty>
concept not_a_const_iterator = not_a_const_iterator_impl<Ty>::value;

template <class>
struct Basic_const_iterator_category
{};

template <std::forward_iterator Iter>
struct Basic_const_iterator_category<Iter>
{
    using iterator_category = std::iterator_traits<Iter>::iterator_category;
};

template <std::input_iterator Iter>
class basic_const_iterator : public Basic_const_iterator_category<Iter>
{
private:
    [[no_unique_address]] Iter current{};

    using Reference        = radr::iter_const_reference_t<Iter>;
    using Rvalue_reference = Iter_const_rvalue_reference_t<Iter>;

    static consteval auto _get_iter_concept() noexcept
    {
        if constexpr (std::contiguous_iterator<Iter>)
        {
            return std::contiguous_iterator_tag{};
        }
        else if constexpr (std::random_access_iterator<Iter>)
        {
            return std::random_access_iterator_tag{};
        }
        else if constexpr (std::bidirectional_iterator<Iter>)
        {
            return std::bidirectional_iterator_tag{};
        }
        else if constexpr (std::forward_iterator<Iter>)
        {
            return std::forward_iterator_tag{};
        }
        else
        {
            return std::input_iterator_tag{};
        }
    }

public:
    using iterator_concept = decltype(_get_iter_concept());
    using value_type       = std::iter_value_t<Iter>;
    using difference_type  = std::iter_difference_t<Iter>;

    // clang-format off
    basic_const_iterator() requires std::default_initializable<Iter> = default;
    // clang-format on

    constexpr basic_const_iterator(Iter other) noexcept(std::is_nothrow_move_constructible_v<Iter>) :
      current(std::move(other))
    {}

    template <std::convertible_to<Iter> Other>
    constexpr basic_const_iterator(basic_const_iterator<Other> other) noexcept(
      std::is_nothrow_constructible_v<Iter, Other>) :
      current(std::move(other.current))
    {}

    template <detail::different_from<basic_const_iterator> Other>
        requires std::convertible_to<Other, Iter>
    constexpr basic_const_iterator(Other && other) noexcept(std::is_nothrow_constructible_v<Iter, Other>) :
      current(std::forward<Other>(other))
    {}

    constexpr Iter const & base() const & noexcept { return current; }

    constexpr Iter base() && noexcept(std::is_nothrow_move_constructible_v<Iter>) { return std::move(current); }

    constexpr Reference operator*() const noexcept(noexcept(static_cast<Reference>(*current)))
    {
        return static_cast<Reference>(*current);
    }

    constexpr auto const * operator->() const noexcept(std::contiguous_iterator<Iter> || noexcept(*current))
        requires std::is_lvalue_reference_v<std::iter_reference_t<Iter>> &&
                 std::same_as<std::remove_cvref_t<std::iter_reference_t<Iter>>, value_type>
    {
        if constexpr (std::contiguous_iterator<Iter>)
        {
            return std::to_address(current);
        }
        else
        {
            return std::addressof(*current);
        }
    }

    constexpr basic_const_iterator & operator++() noexcept(noexcept(++current))
    {
        ++current;
        return *this;
    }

    constexpr void operator++(int) noexcept(noexcept(++current)) { ++current; }

    constexpr basic_const_iterator operator++(int) noexcept(noexcept(++*this) &&
                                                            std::is_nothrow_copy_constructible_v<basic_const_iterator>)
        requires std::forward_iterator<Iter>
    {
        auto Tmp = *this;
        ++*this;
        return Tmp;
    }

    constexpr basic_const_iterator & operator--() noexcept(noexcept(--current))
        requires std::bidirectional_iterator<Iter>
    {
        --current;
        return *this;
    }

    constexpr basic_const_iterator operator--(int) noexcept(noexcept(--*this) &&
                                                            std::is_nothrow_copy_constructible_v<basic_const_iterator>)
        requires std::bidirectional_iterator<Iter>
    {
        auto Tmp = *this;
        --*this;
        return Tmp;
    }

    constexpr basic_const_iterator & operator+=(difference_type const Off) noexcept(noexcept(current += Off))
        requires std::random_access_iterator<Iter>
    {
        current += Off;
        return *this;
    }

    constexpr basic_const_iterator & operator-=(difference_type const Off) noexcept(noexcept(current -= Off))
        requires std::random_access_iterator<Iter>
    {
        current -= Off;
        return *this;
    }

    constexpr Reference operator[](difference_type const Idx) const
      noexcept(noexcept(static_cast<Reference>(current[Idx])))
        requires std::random_access_iterator<Iter>
    {
        return static_cast<Reference>(current[Idx]);
    }

    template <std::sentinel_for<Iter> Sent>
    constexpr bool operator==(Sent const & Se) const noexcept(noexcept((current == Se)))
    {
        return current == Se;
    }

    template <not_a_const_iterator Other>
        requires constant_iterator<Other> && std::convertible_to<Iter const &, Other>
    constexpr operator Other() const & noexcept(std::is_nothrow_convertible_v<Iter const &, Other>)
    {
        return current;
    }

    template <not_a_const_iterator Other>
        requires constant_iterator<Other> && std::convertible_to<Iter, Other>
    constexpr operator Other() && noexcept(std::is_nothrow_convertible_v<Iter, Other>)
    {
        return std::move(current);
    }

    constexpr bool operator<(basic_const_iterator const & Right) const noexcept(noexcept((current < Right.current)))
        requires std::random_access_iterator<Iter>
    {
        return current < Right.current;
    }

    constexpr bool operator>(basic_const_iterator const & Right) const noexcept(noexcept((current > Right.current)))
        requires std::random_access_iterator<Iter>
    {
        return current > Right.current;
    }

    constexpr bool operator<=(basic_const_iterator const & Right) const noexcept(noexcept((current <= Right.current)))
        requires std::random_access_iterator<Iter>
    {
        return current <= Right.current;
    }

    constexpr bool operator>=(basic_const_iterator const & Right) const noexcept(noexcept((current >= Right.current)))
        requires std::random_access_iterator<Iter>
    {
        return current >= Right.current;
    }

    constexpr auto operator<=>(basic_const_iterator const & Right) const noexcept(noexcept(current <=> Right.current))
        requires std::random_access_iterator<Iter> && std::three_way_comparable<Iter>
    {
        return current <=> Right.current;
    }

    template <detail::different_from<basic_const_iterator> Other>
        requires std::random_access_iterator<Iter> && std::totally_ordered_with<Iter, Other>
    constexpr bool operator<(Other const & Right) const noexcept(noexcept((current < Right)))
    {
        return current < Right;
    }

    template <detail::different_from<basic_const_iterator> Other>
        requires std::random_access_iterator<Iter> && std::totally_ordered_with<Iter, Other>
    constexpr bool operator>(Other const & Right) const noexcept(noexcept((current > Right)))
    {
        return current > Right;
    }

    template <detail::different_from<basic_const_iterator> Other>
        requires std::random_access_iterator<Iter> && std::totally_ordered_with<Iter, Other>
    constexpr bool operator<=(Other const & Right) const noexcept(noexcept((current <= Right)))
    {
        return current <= Right;
    }

    template <detail::different_from<basic_const_iterator> Other>
        requires std::random_access_iterator<Iter> && std::totally_ordered_with<Iter, Other>
    constexpr bool operator>=(Other const & Right) const noexcept(noexcept((current >= Right)))
    {
        return current >= Right;
    }

    template <detail::different_from<basic_const_iterator> Other>
        requires std::random_access_iterator<Iter> && std::totally_ordered_with<Iter, Other> &&
                 std::three_way_comparable_with<Iter, Other>
    constexpr auto operator<=>(Other const & Right) const noexcept(noexcept(current <=> Right))
    {
        return current <=> Right;
    }

    template <not_a_const_iterator Other>
        requires std::random_access_iterator<Iter> && std::totally_ordered_with<Iter, Other>
    friend constexpr bool operator<(Other const &                Left,
                                    basic_const_iterator const & Right) noexcept(noexcept((Left < Right.current)))
    {
        return Left < Right.current;
    }

    template <not_a_const_iterator Other>
        requires std::random_access_iterator<Iter> && std::totally_ordered_with<Iter, Other>
    friend constexpr bool operator>(Other const &                Left,
                                    basic_const_iterator const & Right) noexcept(noexcept((Left > Right.current)))
    {
        return Left > Right.current;
    }

    template <not_a_const_iterator Other>
        requires std::random_access_iterator<Iter> && std::totally_ordered_with<Iter, Other>
    friend constexpr bool operator<=(Other const &                Left,
                                     basic_const_iterator const & Right) noexcept(noexcept((Left <= Right.current)))
    {
        return Left <= Right.current;
    }

    template <not_a_const_iterator Other>
        requires std::random_access_iterator<Iter> && std::totally_ordered_with<Iter, Other>
    friend constexpr bool operator>=(Other const &                Left,
                                     basic_const_iterator const & Right) noexcept(noexcept((Left >= Right.current)))
    {
        return Left >= Right.current;
    }

    friend constexpr basic_const_iterator operator+(
      basic_const_iterator const & It,
      difference_type const        Off) noexcept(noexcept(basic_const_iterator{It.current + Off}))
        requires std::random_access_iterator<Iter>
    {
        return basic_const_iterator{It.current + Off};
    }

    friend constexpr basic_const_iterator operator+(
      difference_type const        Off,
      basic_const_iterator const & It) noexcept(noexcept(basic_const_iterator{It.current + Off}))
        requires std::random_access_iterator<Iter>
    {
        return basic_const_iterator{It.current + Off};
    }

    friend constexpr basic_const_iterator operator-(
      basic_const_iterator const & It,
      difference_type const        Off) noexcept(noexcept(basic_const_iterator{It.current - Off}))
        requires std::random_access_iterator<Iter>
    {
        return basic_const_iterator{It.current - Off};
    }

    template <std::sized_sentinel_for<Iter> Sent>
    constexpr difference_type operator-(Sent const & Se) const noexcept(noexcept(current - Se))
    {
        return current - Se;
    }

    template <not_a_const_iterator Sent>
        requires std::sized_sentinel_for<Sent, Iter>
    friend constexpr difference_type operator-(Sent const &                 Se,
                                               basic_const_iterator const & It) noexcept(noexcept(Se - It.current))
    {
        return Se - It.current;
    }

    friend constexpr Rvalue_reference iter_move(basic_const_iterator const & It) noexcept(
      noexcept(static_cast<Rvalue_reference>(std::ranges::iter_move(It.current))))
    {
        return static_cast<Rvalue_reference>(std::ranges::iter_move(It.current));
    }
};

} // namespace radr::detail

namespace radr
{

template <std::input_iterator Iter>
constexpr const_iterator<Iter> make_const_iterator(Iter It) noexcept(
  std::is_nothrow_constructible_v<const_iterator<Iter>, Iter &>)
{
    return It;
}

template <std::semiregular Sent>
constexpr const_sentinel<Sent> make_const_sentinel(Sent Se) noexcept(
  std::is_nothrow_constructible_v<const_sentinel<Sent>, Sent &>)
{
    return Se;
}

} // namespace radr

namespace std
{

template <class Ty1, common_with<Ty1> Ty2>
    requires std::input_iterator<std::common_type_t<Ty1, Ty2>>
struct common_type<radr::detail::basic_const_iterator<Ty1>, Ty2>
{
    using type = radr::detail::basic_const_iterator<std::common_type_t<Ty1, Ty2>>;
};

template <class Ty1, common_with<Ty1> Ty2>
    requires std::input_iterator<std::common_type_t<Ty1, Ty2>>
struct common_type<Ty2, radr::detail::basic_const_iterator<Ty1>>
{
    using type = radr::detail::basic_const_iterator<std::common_type_t<Ty1, Ty2>>;
};

template <class Ty1, common_with<Ty1> Ty2>
    requires std::input_iterator<std::common_type_t<Ty1, Ty2>>
struct common_type<radr::detail::basic_const_iterator<Ty1>, radr::detail::basic_const_iterator<Ty2>>
{
    using type = radr::detail::basic_const_iterator<std::common_type_t<Ty1, Ty2>>;
};

} // namespace std
