#include <cstddef>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/custom/subborrow.hpp>
#include <radr/standalone/repeat_rng.hpp>

#include "radr/standalone/single_rng.hpp"

//---------------------------------------------------------------------
// deduction guides
//---------------------------------------------------------------------

TEST(repeat, deduction_guides)
{
    int i = 3;
    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(std::ref(i))),
                     (radr::repeat_rng<int, std::unreachable_sentinel_t, radr::single_rng_storage::indirect>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(std::ref(i), std::unreachable_sentinel)),
                     (radr::repeat_rng<int, std::unreachable_sentinel_t, radr::single_rng_storage::indirect>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(std::ref(i), 5)),
                     (radr::repeat_rng<int, ptrdiff_t, radr::single_rng_storage::indirect>));

    EXPECT_SAME_TYPE(
      decltype(radr::repeat_rng(std::vector{3})),
      (radr::repeat_rng<std::vector<int>, std::unreachable_sentinel_t, radr::single_rng_storage::in_range>));

    EXPECT_SAME_TYPE(
      decltype(radr::repeat_rng(std::vector{3}, std::unreachable_sentinel)),
      (radr::repeat_rng<std::vector<int>, std::unreachable_sentinel_t, radr::single_rng_storage::in_range>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(std::vector{3}, 5)),
                     (radr::repeat_rng<std::vector<int>, ptrdiff_t, radr::single_rng_storage::in_range>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(3)),
                     (radr::repeat_rng<int, std::unreachable_sentinel_t, radr::single_rng_storage::in_iterator>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(3, std::unreachable_sentinel)),
                     (radr::repeat_rng<int, std::unreachable_sentinel_t, radr::single_rng_storage::in_iterator>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(3, 5)),
                     (radr::repeat_rng<int, ptrdiff_t, radr::single_rng_storage::in_iterator>));
}

template <typename repeat_rng_t>
struct repeat_rng : public testing::Test
{};

template <typename TVal_, typename Bound_, radr::single_rng_storage storage_>
struct repeat_rng<radr::repeat_rng<TVal_, Bound_, storage_>> : public testing::Test
{
    using TVal                    = TVal_;
    using Bound                   = Bound_;
    static constexpr auto storage = storage_;

    static constexpr bool bounded  = !std::same_as<Bound, std::unreachable_sentinel_t>;
    static constexpr bool borrowed = storage != radr::single_rng_storage::in_range;
    static constexpr bool is_const = std::is_const_v<TVal> || storage == radr::single_rng_storage::in_iterator;
};

using rng_types =
  ::testing::Types<radr::repeat_rng<int, ptrdiff_t, radr::single_rng_storage::indirect>,
                   radr::repeat_rng<int, ptrdiff_t, radr::single_rng_storage::in_range>,
                   radr::repeat_rng<int, ptrdiff_t, radr::single_rng_storage::in_iterator>,
                   radr::repeat_rng<int const, ptrdiff_t, radr::single_rng_storage::indirect>,
                   radr::repeat_rng<int const, ptrdiff_t, radr::single_rng_storage::in_range>,
                   radr::repeat_rng<int const, ptrdiff_t, radr::single_rng_storage::in_iterator>,
                   radr::repeat_rng<int, std::unreachable_sentinel_t, radr::single_rng_storage::in_range>,
                   radr::repeat_rng<int, std::unreachable_sentinel_t, radr::single_rng_storage::indirect>,
                   radr::repeat_rng<int, std::unreachable_sentinel_t, radr::single_rng_storage::in_iterator>,
                   radr::repeat_rng<int const, std::unreachable_sentinel_t, radr::single_rng_storage::in_range>,
                   radr::repeat_rng<int const, std::unreachable_sentinel_t, radr::single_rng_storage::indirect>,
                   radr::repeat_rng<int const, std::unreachable_sentinel_t, radr::single_rng_storage::in_iterator>>;

TYPED_TEST_SUITE(repeat_rng, rng_types);

TYPED_TEST(repeat_rng, concepts)
{
    using R = TypeParam;

    EXPECT_TRUE(std::ranges::random_access_range<R>);
    EXPECT_EQ(std::ranges::borrowed_range<R>, TestFixture::borrowed);
    EXPECT_EQ(std::ranges::common_range<R>, TestFixture::bounded);
    EXPECT_EQ(std::ranges::sized_range<R>, TestFixture::bounded);

    if constexpr (TestFixture::storage == radr::single_rng_storage::in_iterator)
    {
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<R>, int);
    }
    else if constexpr (TestFixture::is_const)
    {
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<R>, int const &);
    }
    else
    {
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<R>, int &);
    }

    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<R, std::vector<int> /*irrelevant*/>();
}

TYPED_TEST(repeat_rng, no_arg)
{
    TypeParam r{};

    if constexpr (TestFixture::bounded)
    {
        EXPECT_EQ(r.size(), 0ull);
        EXPECT_EQ(r.empty(), true);
    }
    else if constexpr (TestFixture::storage == radr::single_rng_storage::indirect)
    {
        /* this is the only dangerous case */
        EXPECT_EQ(r.empty(), false);
        // deref would be UB
    }
    else
    {
        EXPECT_EQ(r.empty(), false);
        EXPECT_EQ(*r.begin(), 0);
        EXPECT_EQ(r[2], 0);
    }
}

TYPED_TEST(repeat_rng, one_arg)
{
    int       i = 3;
    TypeParam r{i};

    if constexpr (TestFixture::bounded)
    {
        EXPECT_EQ(r.size(), 0ull);
        EXPECT_EQ(r.empty(), true);
    }
    else
    {
        EXPECT_EQ(r.empty(), false);
        EXPECT_EQ(*r.begin(), 3);
        EXPECT_EQ(r[2], 3);
    }
}

TYPED_TEST(repeat_rng, two_arg)
{
    int i = 3;

    if constexpr (TestFixture::bounded)
    {
        TypeParam r{i, 5};
        EXPECT_EQ(r.size(), 5ull);
        EXPECT_EQ(r.empty(), false);
        EXPECT_EQ(*r.begin(), 3);
        EXPECT_EQ(r[2], 3);

        if constexpr (!TestFixture::is_const)
        {
            r[2] = 42; // changing one value, changes all
            EXPECT_EQ(*r.begin(), 42);
        }
    }
    else
    {
        TypeParam r{i, std::unreachable_sentinel};
        EXPECT_EQ(r.empty(), false);
        EXPECT_EQ(*r.begin(), 3);
        EXPECT_EQ(r[2], 3);

        if constexpr (!TestFixture::is_const)
        {
            r[2] = 42; // changing one value, changes all
            EXPECT_EQ(*r.begin(), 42);
        }
    }
}

TYPED_TEST(repeat_rng, iterators)
{
    int i = 3;

    if constexpr (TestFixture::bounded)
    {
        TypeParam r{i, 5};

        auto it = r.begin();
        it += 4;
        auto it2 = r.end();
        --it2;

        EXPECT_EQ(it, it2);
        EXPECT_EQ(r.end() - r.begin(), 5);
        EXPECT_EQ(r.end() - it, 1);

        ++it2;
        EXPECT_EQ(it2, r.end());
    }
    else
    {
        TypeParam r{i};

        auto it  = r.begin();
        auto it2 = it + 100;

        EXPECT_NE(it, it2);
        EXPECT_EQ(it2 - it, 100);

        it++;
        it2--;
        EXPECT_EQ(it2 - it, 98);
    }
}
