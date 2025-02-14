#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/custom/subborrow.hpp>
#include <radr/factory/empty.hpp>

using R = std::remove_cvref_t<decltype(radr::empty<int>)>;
using C = std::remove_cvref_t<decltype(radr::empty<int const>)>;

TEST(empty_rng, concepts)
{
    EXPECT_TRUE(std::ranges::contiguous_range<R>);
    EXPECT_TRUE(std::ranges::contiguous_range<C>);
    EXPECT_TRUE(std::ranges::common_range<R>);
    EXPECT_TRUE(std::ranges::common_range<C>);
    EXPECT_TRUE(std::ranges::borrowed_range<R>);
    EXPECT_TRUE(std::ranges::borrowed_range<C>);

    EXPECT_SAME_TYPE(radr::iterator_t<R>, int *);
    EXPECT_SAME_TYPE(radr::iterator_t<C>, int const *);

    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<R, std::vector<int> /*irrelevant*/>();
    radr::test::generic_adaptor_checks<C, std::vector<int> /*irrelevant*/>();

    EXPECT_EQ(sizeof(R), 2);
    EXPECT_EQ(sizeof(C), 2);
}

TEST(empty_rng, members)
{
    R r;
    EXPECT_EQ(r.begin(), nullptr);
    EXPECT_EQ(r.end(), nullptr);
    EXPECT_EQ(r.data(), nullptr);
    EXPECT_EQ(r.size(), 0ull);
    EXPECT_EQ(r.empty(), true);

    C c;
    EXPECT_EQ(c.begin(), nullptr);
    EXPECT_EQ(c.end(), nullptr);
    EXPECT_EQ(c.data(), nullptr);
    EXPECT_EQ(c.size(), 0ull);
    EXPECT_EQ(c.empty(), true);
}

TEST(empty_rng, subborrow)
{
    R r;

    auto sub1 = radr::borrow(r);
    EXPECT_SAME_TYPE(decltype(sub1), R);

    auto sub2 = radr::borrow(std::as_const(r));
    EXPECT_SAME_TYPE(decltype(sub2), C);

    auto sub3 = radr::subborrow(r, (int *)nullptr, (int *)nullptr);
    EXPECT_SAME_TYPE(decltype(sub3), decltype(r));

    auto sub4 = radr::subborrow(r, (int *)nullptr, (int *)nullptr, 0ul);
    EXPECT_SAME_TYPE(decltype(sub4), decltype(r));
}
