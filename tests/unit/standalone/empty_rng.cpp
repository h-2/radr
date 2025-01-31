#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/custom/subborrow.hpp>
#include <radr/standalone/empty_rng.hpp>

TEST(empty_rng, concepts)
{
    using R = radr::empty_rng<int>;
    using C = radr::empty_rng<int const>;

    EXPECT_TRUE(std::ranges::contiguous_range<R>);
    EXPECT_TRUE(std::ranges::contiguous_range<C>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<R>, int *);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<C>, int const *);

    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<R, std::vector<int> /*irrelevant*/>();
    radr::test::generic_adaptor_checks<C, std::vector<int> /*irrelevant*/>();
}

TEST(empty_rng, members)
{
    radr::empty_rng<int> r;
    EXPECT_EQ(r.begin(), nullptr);
    EXPECT_EQ(r.end(), nullptr);
    EXPECT_EQ(r.data(), nullptr);
    EXPECT_EQ(r.size(), 0ull);
    EXPECT_EQ(r.empty(), true);

    radr::empty_rng<int const> c;
    EXPECT_EQ(c.begin(), nullptr);
    EXPECT_EQ(c.end(), nullptr);
    EXPECT_EQ(c.data(), nullptr);
    EXPECT_EQ(c.size(), 0ull);
    EXPECT_EQ(c.empty(), true);
}

TEST(empty_rng, subborrow)
{
    radr::empty_rng<int> r;

    auto sub1 = radr::borrow(r);
    EXPECT_SAME_TYPE(decltype(sub1), decltype(r));

    auto sub2 = radr::borrow(std::as_const(r));
    EXPECT_SAME_TYPE(decltype(sub2), radr::empty_rng<int const>);

    auto sub3 = radr::subborrow(r, (int *)nullptr, (int *)nullptr);
    EXPECT_SAME_TYPE(decltype(sub3), decltype(r));

    auto sub4 = radr::subborrow(r, (int *)nullptr, (int *)nullptr, 0ul);
    EXPECT_SAME_TYPE(decltype(sub4), decltype(r));
}
