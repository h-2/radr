#pragma once

#include <gtest/gtest.h>

#include <radr/concepts.hpp>
#include <radr/range_access.hpp>

namespace radr::test
{

// range category
enum class range_cat : uint8_t
{
    input,
    fwd,
    bidi,
    ra,
    contig
};

struct concept_expectations
{
    range_cat cat = range_cat::input;

    bool sized    = false;
    bool infinite = false;

    bool common   = false;
    bool constant = false;
    bool mut      = false;

    bool borrowed = false;
};

template <typename rad_t>
void check_adaptor_concepts(concept_expectations e)
{
    /* always true for multi-pass adaptors */
    EXPECT_TRUE(radr::mp_range<rad_t>);
    // std::regular:
    EXPECT_TRUE(std::default_initializable<rad_t>);
    EXPECT_TRUE(std::equality_comparable<rad_t>);
    EXPECT_TRUE(std::copyable<rad_t>);

    /* always true for multi-pass adaptors */
    using crad_t = rad_t const;
    EXPECT_TRUE(radr::constant_range<crad_t>);
    EXPECT_TRUE(radr::const_symmetric_range<crad_t>);
    EXPECT_TRUE(std::default_initializable<crad_t>);
    EXPECT_TRUE(std::equality_comparable<crad_t>);
    // not copyable because const is not assignable

    /* iterator compatibility */
    EXPECT_TRUE((std::convertible_to<radr::iterator_t<rad_t>, radr::iterator_t<crad_t>>));
    EXPECT_TRUE((std::convertible_to<radr::sentinel_t<rad_t>, radr::sentinel_t<crad_t>>));

    /* specific checks */
    EXPECT_EQ(std::ranges::bidirectional_range<rad_t>, ((uint8_t)e.cat >= (uint8_t)range_cat::bidi));
    EXPECT_EQ(std::ranges::random_access_range<rad_t>, ((uint8_t)e.cat >= (uint8_t)range_cat::ra));
    EXPECT_EQ(std::ranges::contiguous_range<rad_t>, (e.cat == range_cat::contig));
    EXPECT_EQ(std::ranges::sized_range<rad_t>, e.sized);
    EXPECT_EQ(radr::infinite_mp_range<rad_t>, e.infinite);
    EXPECT_EQ(radr::common_range<rad_t>, e.common);
    EXPECT_EQ(radr::constant_range<rad_t>, e.constant);
    EXPECT_EQ(radr::mutable_range<rad_t>, e.mut);
    EXPECT_EQ(std::ranges::borrowed_range<rad_t>, e.borrowed);
}

/* all our forward range adaptors model these concepts*/
template <typename rad_t, typename container_t>
inline void generic_adaptor_checks()
{
    using crad_t = rad_t const;

    //TODO add type printing to improve diagnostics in case of failure
    EXPECT_TRUE(radr::mp_range<rad_t>);
    EXPECT_TRUE(!radr::const_symmetric_range<container_t> || radr::const_symmetric_range<rad_t>);
    EXPECT_TRUE(std::default_initializable<rad_t>);
    EXPECT_TRUE(std::equality_comparable<rad_t>);
    EXPECT_TRUE(std::copyable<rad_t>);

    EXPECT_TRUE(radr::constant_range<crad_t>);
    EXPECT_TRUE(radr::const_symmetric_range<crad_t>);
    EXPECT_TRUE(std::default_initializable<crad_t>);
    EXPECT_TRUE(std::equality_comparable<crad_t>);
    // not copyable because const is not assignable

    EXPECT_TRUE((std::convertible_to<radr::iterator_t<rad_t>, radr::iterator_t<crad_t>>));
    EXPECT_TRUE((std::convertible_to<radr::sentinel_t<rad_t>, radr::sentinel_t<crad_t>>));
}

} // namespace radr::test
