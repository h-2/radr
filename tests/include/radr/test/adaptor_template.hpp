#pragma once

#include <gtest/gtest.h>

#include <radr/concepts.hpp>
#include <radr/range_access.hpp>

namespace radr::test
{

/* for all our forward range adaptors model these concepts*/
template <typename rad_t, typename crad_t, typename container_t>
inline void generic_adaptor_checks()
{
    //TODO add type printing to improve diagnostics in case of failure
    EXPECT_TRUE(radr::const_iterable_range<rad_t>);
    EXPECT_TRUE(!radr::const_symmetric_range<container_t> || radr::const_symmetric_range<rad_t>);
    EXPECT_TRUE(std::default_initializable<rad_t>);
    EXPECT_TRUE(std::equality_comparable<rad_t>);
    EXPECT_TRUE(std::copyable<rad_t>);

    EXPECT_TRUE(radr::const_symmetric_range<crad_t>);
    EXPECT_TRUE(std::default_initializable<crad_t>);
    EXPECT_TRUE(std::equality_comparable<crad_t>);
    // not copyable because const is not assignable

    EXPECT_TRUE((std::convertible_to<radr::iterator_t<rad_t>, radr::iterator_t<crad_t>>));
    EXPECT_TRUE((std::convertible_to<radr::sentinel_t<rad_t>, radr::sentinel_t<crad_t>>));
}

} // namespace radr::test
