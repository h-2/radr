#include <array>
#include <deque>
#include <forward_list>
#include <iterator>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/counted.hpp>
#include <radr/factory/iota.hpp>

TEST(counted, Basic)
{
    int              arr[]{1, 2, 3, 4, 5};
    auto             rng = radr::counted(arr, 3);
    std::vector<int> expected{1, 2, 3};
    EXPECT_RANGE_EQ(rng, expected);
}

TEST(counted, EdgeCases_ZeroCount)
{
    int              arr[]{1, 2, 3};
    auto             rng = radr::counted(arr, 0);
    std::vector<int> expected{};
    EXPECT_RANGE_EQ(rng, expected);
}

TEST(counted, EdgeCases_ExactEnd)
{
    int              arr[]{7, 8, 9, 10};
    auto             rng = radr::counted(arr + 2, 2);
    std::vector<int> expected{9, 10};
    EXPECT_RANGE_EQ(rng, expected);
}

TEST(counted, ForwardIterator)
{
    std::forward_list<int> flist{5, 6, 7, 8};
    auto                   it = flist.begin();
    std::ranges::advance(it, 1);

    auto             rng = radr::counted(it, 2);
    std::vector<int> expected{6, 7};

    EXPECT_RANGE_EQ(rng, expected);

    EXPECT_TRUE(std::regular<decltype(rng)>);
    EXPECT_TRUE(radr::mp_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(rng)>);

    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(rng)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(rng)>);
    EXPECT_FALSE(std::ranges::contiguous_range<decltype(rng)>);

    EXPECT_FALSE(radr::common_range<decltype(rng)>);
}

TEST(counted, BidirectionalIterator)
{
    std::list<int> lst{11, 12, 13, 14, 15};
    auto           it = lst.begin();
    std::ranges::advance(it, 1);

    auto             rng = radr::counted(it, 3);
    std::vector<int> expected{12, 13, 14};

    EXPECT_RANGE_EQ(rng, expected);

    EXPECT_TRUE(std::regular<decltype(rng)>);
    EXPECT_TRUE(radr::mp_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(rng)>);

    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(rng)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(rng)>);
    EXPECT_FALSE(std::ranges::contiguous_range<decltype(rng)>);

    EXPECT_FALSE(radr::common_range<decltype(rng)>);
}

TEST(counted, RandomAccessIterator)
{
    std::deque<int>  vec{3, 1, 4, 1, 5};
    auto             rng = radr::counted(vec.begin() + 2, 2);
    std::vector<int> expected{4, 1};
    EXPECT_RANGE_EQ(rng, expected);

    EXPECT_TRUE(std::regular<decltype(rng)>);
    EXPECT_TRUE(radr::mp_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(rng)>);

    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::random_access_range<decltype(rng)>);
    EXPECT_FALSE(std::ranges::contiguous_range<decltype(rng)>);

    EXPECT_TRUE(radr::common_range<decltype(rng)>);
}

TEST(counted, ContiguousIterator)
{
    std::vector<int> vec{3, 1, 4, 1, 5};
    auto             rng = radr::counted(vec.begin() + 2, 2);
    std::vector<int> expected{4, 1};
    EXPECT_RANGE_EQ(rng, expected);

    EXPECT_TRUE(std::regular<decltype(rng)>);
    EXPECT_TRUE(radr::mp_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(rng)>);

    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::random_access_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::contiguous_range<decltype(rng)>);

    EXPECT_TRUE(radr::common_range<decltype(rng)>);
}

TEST(counted, Constexpr)
{
    static constexpr std::array<int, 4> a{5, 6, 7, 8};
    constexpr auto                      rng = radr::counted(a.begin() + 1, 2);

    static_assert(rng.size() == 2);
    static_assert(*rng.begin() == 6);
}

TEST(counted, Constexpr_Zero)
{
    static constexpr std::array<int, 1> a{1};
    constexpr auto                      rng = radr::counted(a.begin(), 0);

    static_assert(rng.size() == 0);
    static_assert(rng.begin() == rng.end());
}

//===========================================================================
// single-pass version
//===========================================================================

TEST(counted_sp, Basic)
{
    int              arr[]{1, 2, 3, 4, 5};
    auto             rng = radr::counted_sp(&*arr, 3);
    std::vector<int> expected{1, 2, 3};
    EXPECT_RANGE_EQ(rng, expected);
}

TEST(counted_sp, EdgeCases_ZeroCount)
{
    int              arr[]{1, 2, 3};
    auto             rng = radr::counted_sp(&*arr, 0);
    std::vector<int> expected{};
    EXPECT_RANGE_EQ(rng, expected);
}

TEST(counted_sp, EdgeCases_ExactEnd)
{
    int              arr[]{7, 8, 9, 10};
    auto             rng = radr::counted_sp(&*arr + 2, 2);
    std::vector<int> expected{9, 10};
    EXPECT_RANGE_EQ(rng, expected);
}

TEST(counted_sp, InputIterator)
{
    auto gen = radr::iota_sp(5, 9);
    auto it  = gen.begin();
    std::ranges::advance(it, 1);

    auto             rng = radr::counted_sp(it, 2);
    std::vector<int> expected{6, 7};

    EXPECT_RANGE_EQ(rng, expected);
#ifdef __cpp_lib_generator
    EXPECT_SAME_TYPE(decltype(rng), (radr::generator<int &&, int, void>));
#else
    EXPECT_SAME_TYPE(decltype(rng), (radr::generator<int>));
#endif
}

TEST(counted_sp, ForwardIterator)
{
    std::forward_list<int> flist{5, 6, 7, 8};
    auto                   it = flist.begin();
    std::ranges::advance(it, 1);

    auto             rng = radr::counted_sp(it, 2);
    std::vector<int> expected{6, 7};

    EXPECT_RANGE_EQ(rng, expected);
    EXPECT_SAME_TYPE(decltype(rng), (radr::generator<int &, int>));
}

TEST(counted_sp, BidirectionalIterator)
{
    std::list<int> lst{11, 12, 13, 14, 15};
    auto           it = lst.begin();
    std::ranges::advance(it, 1);

    auto             rng = radr::counted_sp(it, 3);
    std::vector<int> expected{12, 13, 14};

    EXPECT_RANGE_EQ(rng, expected);
    EXPECT_SAME_TYPE(decltype(rng), (radr::generator<int &, int>));
}

TEST(counted_sp, RandomAccessIterator)
{
    std::deque<int>  vec{3, 1, 4, 1, 5};
    auto             rng = radr::counted_sp(vec.begin() + 2, 2);
    std::vector<int> expected{4, 1};

    EXPECT_RANGE_EQ(rng, expected);
    EXPECT_SAME_TYPE(decltype(rng), (radr::generator<int &, int>));
}

TEST(counted_sp, ContiguousIterator)
{
    std::vector<int> vec{3, 1, 4, 1, 5};
    auto             rng = radr::counted_sp(vec.begin() + 2, 2);
    std::vector<int> expected{4, 1};

    EXPECT_RANGE_EQ(rng, expected);
    EXPECT_SAME_TYPE(decltype(rng), (radr::generator<int &, int>));
}

// generator not yet constexpr
