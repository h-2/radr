// Test-suite for radr::pairwise_transform, which is an alias for radr::adjacent_transform<2>.
// The behaviour itself is covered by adjacent_transform.cpp; this file only pins the aliasing down.
// NOTE: deliberately NOT guarded by RADR_FEATURE_ZIP; radr::pairwise_transform is available in C++20.

#include <functional>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/rad/pairwise_transform.hpp>

using radr::test::range_cat;

inline std::vector<int> vec{1, 2, 3, 4, 5};

inline constexpr auto add = [](int a, int b)
{
    return a + b;
};

inline std::vector<int> const pair_sums{3, 5, 7, 9};

TEST(pairwise_transform_mp, is_adjacent_transform_2)
{
    EXPECT_SAME_TYPE(decltype(radr::pairwise_transform), decltype(radr::adjacent_transform<2>));

    auto pw  = std::ref(vec) | radr::pairwise_transform(add);
    auto adj = std::ref(vec) | radr::adjacent_transform<2>(add);

    EXPECT_SAME_TYPE(decltype(pw), decltype(adj));
    EXPECT_RANGE_EQ(pw, adj);
}

TEST(pairwise_transform_mp, contig_sized)
{
    auto pw = std::ref(vec) | radr::pairwise_transform(add);

    EXPECT_RANGE_EQ(pw, pair_sums);
    radr::test::check_adaptor_concepts<decltype(pw)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(pairwise_transform_mp, bidi_common)
{
    std::list<int> lst{1, 2, 3, 4, 5};
    auto           pw = std::ref(lst) | radr::pairwise_transform(add);

    EXPECT_RANGE_EQ(pw, pair_sums);
    radr::test::check_adaptor_concepts<decltype(pw)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .borrowed = true});
}

TEST(pairwise_transform_mp, empty)
{
    std::vector<int> v{};
    auto             pw = std::ref(v) | radr::pairwise_transform(add);

    EXPECT_RANGE_EQ(pw, (std::vector<int>{}));
    EXPECT_TRUE(pw.begin() == pw.end());
}

TEST(pairwise_transform_mp, deep_copy)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::pairwise_transform(add));

    T cpy;

    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::pairwise_transform(add);
        EXPECT_RANGE_EQ(own, pair_sums);
        cpy = own;
    }

    EXPECT_RANGE_EQ(cpy, pair_sums);
}
