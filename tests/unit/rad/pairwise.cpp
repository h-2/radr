// Test-suite for radr::pairwise, which is an alias for radr::adjacent<2>.
// The behaviour itself is covered by adjacent.cpp; this file only pins the aliasing down.

#include <gtest/gtest.h>

#include <radr/version.hpp>

#if !RADR_FEATURE_ZIP

TEST(dummy, skipped_because_no_cpp23)
{
    GTEST_SKIP() << "Requires C++23";
}

#else

#    include <functional>
#    include <list>
#    include <ranges>
#    include <tuple>
#    include <vector>

#    include <radr/test/adaptor_template.hpp>
#    include <radr/test/gtest_helpers.hpp>

#    include <radr/rad/pairwise.hpp>

using radr::test::range_cat;

inline std::vector<int> vec{1, 2, 3, 4, 5};

inline std::vector<std::tuple<int, int>> const pairs{
  {1, 2},
  {2, 3},
  {3, 4},
  {4, 5}
};

TEST(pairwise_mp, is_adjacent_2)
{
    EXPECT_SAME_TYPE(decltype(radr::pairwise), decltype(radr::adjacent<2>));

    auto pw  = std::ref(vec) | radr::pairwise;
    auto adj = std::ref(vec) | radr::adjacent<2>;

    EXPECT_SAME_TYPE(decltype(pw), decltype(adj));
    EXPECT_RANGE_EQ(pw, adj);
}

TEST(pairwise_mp, contig_sized)
{
    auto pw = std::ref(vec) | radr::pairwise;

    EXPECT_RANGE_EQ(pw, pairs);
    // std::vector input is contiguous, but pairwise yields tuples -> .cat is ra, not contig
    radr::test::check_adaptor_concepts<decltype(pw)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(pairwise_mp, bidi_common)
{
    std::list<int> lst{1, 2, 3, 4, 5};
    auto           pw = std::ref(lst) | radr::pairwise;

    EXPECT_RANGE_EQ(pw, pairs);
    radr::test::check_adaptor_concepts<decltype(pw)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(pairwise_mp, empty)
{
    std::vector<int> v{};
    auto             pw = std::ref(v) | radr::pairwise;

    EXPECT_RANGE_EQ(pw, (std::vector<std::tuple<int, int>>{}));
    EXPECT_TRUE(pw.begin() == pw.end());
}

TEST(pairwise_mp, deep_copy)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::pairwise);

    T cpy;

    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::pairwise;
        EXPECT_RANGE_EQ(own, pairs);
        cpy = own;
    }

    EXPECT_RANGE_EQ(cpy, pairs);
}

#endif
