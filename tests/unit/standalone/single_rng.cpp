#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/custom/subborrow.hpp>
#include <radr/standalone/single_rng.hpp>

//---------------------------------------------------------------------
// deduction guides
//---------------------------------------------------------------------

TEST(single, deduction_guides)
{
    int i = 3;
    EXPECT_SAME_TYPE(decltype(radr::single_rng(std::ref(i))),
                     (radr::single_rng<int, radr::single_rng_storage::indirect>));

    EXPECT_SAME_TYPE(decltype(radr::single_rng(std::vector{3})),
                     (radr::single_rng<std::vector<int>, radr::single_rng_storage::in_range>));

    EXPECT_SAME_TYPE(decltype(radr::single_rng(3)),
                     (radr::single_rng<int const, radr::single_rng_storage::in_iterator>));
}

//---------------------------------------------------------------------
// indirect
//---------------------------------------------------------------------

TEST(single_indirect, concepts)
{
    using R = radr::single_rng<int, radr::single_rng_storage::indirect>;
    EXPECT_TRUE(std::ranges::contiguous_range<R>);
    EXPECT_TRUE(std::ranges::borrowed_range<R>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<R>, int *);
    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<R, std::vector<int> /*irrelevant*/>();
}

TEST(single_indirect, concepts_const)
{
    using C = radr::single_rng<int const, radr::single_rng_storage::indirect>;
    EXPECT_TRUE(std::ranges::contiguous_range<C>);
    EXPECT_TRUE(std::ranges::borrowed_range<C>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<C>, int const *);
    radr::test::generic_adaptor_checks<C, std::vector<int> /*irrelevant*/>();
}

TEST(single_indirect, members)
{
    int                                                       i = 3;
    radr::single_rng<int, radr::single_rng_storage::indirect> r{i};
    EXPECT_EQ(*r.begin(), 3);
    EXPECT_EQ(*r.data(), 3);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_indirect, members_const)
{
    int                                                             i = 3;
    radr::single_rng<int const, radr::single_rng_storage::indirect> c{i};
    EXPECT_EQ(*c.begin(), 3);
    EXPECT_EQ(*c.data(), 3);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

TEST(single_indirect, default_)
{
    radr::single_rng<int, radr::single_rng_storage::indirect> r{};

    EXPECT_EQ(r.begin(), nullptr);
    // Dereferencing is UB
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_indirect, default_const)
{
    radr::single_rng<int const, radr::single_rng_storage::indirect> c{};

    EXPECT_EQ(c.begin(), nullptr);
    // Dereferencing is UB
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

//---------------------------------------------------------------------
// in_range
//---------------------------------------------------------------------

TEST(single_in_range, concepts)
{
    using R = radr::single_rng<int, radr::single_rng_storage::in_range>;
    EXPECT_TRUE(std::ranges::contiguous_range<R>);
    EXPECT_FALSE(std::ranges::borrowed_range<R>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<R>, int *);
    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<R, std::vector<int> /*irrelevant*/>();
}

TEST(single_in_range, concepts_const)
{
    using C = radr::single_rng<int const, radr::single_rng_storage::in_range>;
    EXPECT_TRUE(std::ranges::contiguous_range<C>);
    EXPECT_FALSE(std::ranges::borrowed_range<C>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<C>, int const *);
    radr::test::generic_adaptor_checks<C, std::vector<int> /*irrelevant*/>();
}

TEST(single_in_range, members)
{
    radr::single_rng<int, radr::single_rng_storage::in_range> r{3};
    EXPECT_EQ(*r.begin(), 3);
    EXPECT_EQ(*r.data(), 3);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_in_range, members_const)
{
    radr::single_rng<int const, radr::single_rng_storage::in_range> c{3};
    EXPECT_EQ(*c.begin(), 3);
    EXPECT_EQ(*c.data(), 3);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

TEST(single_in_range, default_)
{
    radr::single_rng<int, radr::single_rng_storage::in_range> r{};

    EXPECT_EQ(*r.begin(), 0);
    EXPECT_EQ(*r.data(), 0);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_in_range, default_const)
{
    radr::single_rng<int const, radr::single_rng_storage::in_range> c{};

    EXPECT_EQ(*c.begin(), 0);
    EXPECT_EQ(*c.data(), 0);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

//---------------------------------------------------------------------
// in_iterator
//---------------------------------------------------------------------

TEST(single_in_iterator, concepts)
{
    using R = radr::single_rng<int, radr::single_rng_storage::in_iterator>;
    EXPECT_FALSE(std::ranges::contiguous_range<R>);
    EXPECT_TRUE(std::ranges::random_access_range<R>);
    EXPECT_TRUE(std::ranges::borrowed_range<R>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<R>, radr::detail::small_single_iterator<int>);
    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<R, std::vector<int> /*irrelevant*/>();
}

TEST(single_in_iterator, concepts_const)
{
    using C = radr::single_rng<int const, radr::single_rng_storage::in_iterator>;
    EXPECT_FALSE(std::ranges::contiguous_range<C>);
    EXPECT_TRUE(std::ranges::random_access_range<C>);
    EXPECT_TRUE(std::ranges::borrowed_range<C>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<C>, radr::detail::small_single_iterator<int>);
    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<C, std::vector<int> /*irrelevant*/>();
}

TEST(single_in_iterator, members)
{
    radr::single_rng<int, radr::single_rng_storage::in_iterator> r{3};
    EXPECT_EQ(*r.begin(), 3);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_in_iterator, members_const)
{
    radr::single_rng<int const, radr::single_rng_storage::in_iterator> c{3};
    EXPECT_EQ(*c.begin(), 3);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

TEST(single_in_iterator, default_)
{
    radr::single_rng<int, radr::single_rng_storage::in_iterator> r{};

    EXPECT_EQ(*r.begin(), 0);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_in_iterator, default_const)
{
    radr::single_rng<int const, radr::single_rng_storage::in_iterator> c{};

    EXPECT_EQ(*c.begin(), 0);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}
