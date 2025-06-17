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

#include <istream>

#include "radr/generator.hpp"

namespace radr::detail
{

template <class Val, class CharT, class Traits>
concept stream_extractable = requires(std::basic_istream<CharT, Traits> & is, Val & t) { is >> t; };

}

namespace radr
{

/*!\brief A range factory that produces a range over the values from an istream.
 * \tparam Val The type to extract into.
 * \tparam CharT The character type of the stream.
 * \tparam Traits CharTraits of the stream.
 * \param[in,out] stream The stream to extract from.
 * \details
 *
 * Values are extracted from the stream as if by `stream >> val;`.
 *
 * The returned range is a specialisation of radr::generator, i.e. it is always a move-only, single-pass range.
 *
 */
template <typename Val, class CharT = char, class Traits = std::char_traits<CharT>>
generator<Val &, Val> istream(std::basic_istream<CharT, Traits> & stream)
{
    static_assert(std::movable<Val>, "The constraints for the Val type of radr::istream were not met.");
    static_assert(detail::stream_extractable<Val, CharT, Traits>,
                  "The Val type cannot be extracted from the stream passed to radr::istream.");

    Val value;
    stream >> value;

    while (stream)
    {
        co_yield value;
        stream >> value;
    }
}

} // namespace radr
