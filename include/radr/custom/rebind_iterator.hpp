// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2024 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>
#include <iterator>
#include <ranges>

#include "../range_access.hpp"
#include "tags.hpp"

namespace radr::detail
{
template <typename It, typename Container, typename BegFn>
constexpr It tag_invoke_rebind_final_impl(It it, Container & container_old, Container & container_new, BegFn beg_fn)
{
    ptrdiff_t offset = std::ranges::distance(beg_fn(container_old), it);
    assert(offset >= 0);

    if constexpr (std::ranges::sized_range<Container>)
    {
        assert(offset <= ptrdiff_t(std::ranges::size(container_old)));
        assert(std::ranges::size(container_old) == std::ranges::size(container_new));
    }

    auto ret = beg_fn(container_new);
    std::ranges::advance(ret, offset);
    return ret;
}

} // namespace radr::detail

namespace radr::custom
{

template <std::semiregular It, typename Container>
constexpr std::counted_iterator<It> tag_invoke(rebind_iterator_tag,
                                               std::counted_iterator<It> it,
                                               Container &               container_old,
                                               Container &               container_new)
{
    ptrdiff_t count = it.count();
    return {tag_invoke(custom::rebind_iterator_tag{}, std::move(it).base(), container_old, container_new), count};
}

//!\brief Recursion anchor for default sentinel (leads to default sentinel).
template <typename Container>
constexpr auto tag_invoke(rebind_iterator_tag, std::default_sentinel_t, Container &, Container &)
{
    return std::default_sentinel;
}

/*!\brief Fallback for iterators that provide `base()` and that can be (re)constructed from base; e.g. std::basic_const_iterator and std::move_iterator.
 * \details
 *
 * Note that we are only checking semiregular here and not forward_iterator, because we need to handle sentinels, too.
 * This may get us some false-positives here, but they should fail in a subsequent step.
 * Potentially, we could also check for no-throw copy construction, which most sentinels should be.
 */
template <std::semiregular It, typename Container>
    requires(!detail::is_iterator_of<It, Container> &&
             requires(It it) {
                 {
                     it.base()
                 };
                 requires std::semiregular<std::remove_cvref_t<decltype(it.base())>>;
                 It(it.base());
             })
constexpr It tag_invoke(rebind_iterator_tag, It it, Container & container_old, Container & container_new)
{
    return It{tag_invoke(custom::rebind_iterator_tag{}, std::move(it).base(), container_old, container_new)};
}

//!\brief Recursion anchor for iterator type.
template <typename Container>
constexpr iterator_t<Container> tag_invoke(rebind_iterator_tag,
                                           iterator_t<Container> it,
                                           Container &           container_old,
                                           Container &           container_new)
{
    return detail::tag_invoke_rebind_final_impl(it, container_old, container_new, radr::begin);
}

//!\brief Recursion anchor for const_iterator type.
template <typename Container>
constexpr const_iterator_t<Container> tag_invoke(rebind_iterator_tag,
                                                 const_iterator_t<Container> it,
                                                 Container &                 container_old,
                                                 Container &                 container_new)
{
    return detail::tag_invoke_rebind_final_impl(it, container_old, container_new, radr::cbegin);
}

//!\brief Recursion anchor for iterator type.
template <typename Container>
    requires(!std::same_as<iterator_t<Container>, std::ranges::iterator_t<Container>>)
constexpr std::ranges::iterator_t<Container> tag_invoke(rebind_iterator_tag,
                                                        std::ranges::iterator_t<Container> it,
                                                        Container &                        container_old,
                                                        Container &                        container_new)
{
    return detail::tag_invoke_rebind_final_impl(it, container_old, container_new, std::ranges::begin);
}

//!\brief Recursion anchor for const_iterator type.
template <typename Container>
    requires(!std::same_as<const_iterator_t<Container>, detail::std_const_iterator_t<Container>>)
constexpr detail::std_const_iterator_t<Container> tag_invoke(rebind_iterator_tag,
                                                             detail::std_const_iterator_t<Container> it,
                                                             Container &                             container_old,
                                                             Container &                             container_new)
{
    return detail::tag_invoke_rebind_final_impl(it, container_old, container_new, std::ranges::cbegin);
}

} // namespace radr::custom

namespace radr
{

template <typename T, typename Container>
concept rebindable_iterator_to =
  requires(T it, Container & container) { tag_invoke(custom::rebind_iterator_tag{}, it, container, container); };

}
