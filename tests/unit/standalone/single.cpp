#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/custom/subborrow.hpp>
#include <radr/standalone/single.hpp>

TEST(single, concepts)
{
    using R = radr::single<int>;
    using C = radr::single<int const>;

    EXPECT_TRUE(std::ranges::contiguous_range<R>);
    EXPECT_TRUE(std::ranges::contiguous_range<C>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<R>, int *);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<C>, int const *);

    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<R, std::vector<int> /*irrelevant*/>();
    radr::test::generic_adaptor_checks<C, std::vector<int> /*irrelevant*/>();
}

TEST(single, members)
{
    radr::single<int> r{3};
    EXPECT_EQ(*r.begin(), 3);
    EXPECT_EQ(*r.data(), 3);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);

    radr::single<int const> c{3};
    EXPECT_EQ(*c.begin(), 3);
    EXPECT_EQ(*c.data(), 3);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}
