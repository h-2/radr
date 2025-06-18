#include <concepts>
#include <limits>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/iota.hpp>
#include <radr/rad/take_while.hpp>
#include <radr/rad/transform.hpp>

// Compile-time iota view is constexpr-constructible and -iterable
constexpr bool constexpr_test()
{
    auto v   = radr::iota(2, 7);
    int  sum = 0;
    for (auto i : v)
        sum += i;
    return sum == (2 + 3 + 4 + 5 + 6);
}
static_assert(constexpr_test());

TEST(iota, Basic)
{
    auto        v = radr::iota(0, 5);
    std::vector expected{0, 1, 2, 3, 4};
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota, EmptyRange)
{
    auto             v = radr::iota(10, 10);
    std::vector<int> expected;
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota, SingleElement)
{
    auto        v = radr::iota(42, 43);
    std::vector expected{42};
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota, NegativeValues)
{
    auto        v = radr::iota(-2, 2);
    std::vector expected{-2, -1, 0, 1};
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota, LargeIntegers)
{
    constexpr auto first = std::numeric_limits<int64_t>::max() - 2;
    constexpr auto last  = std::numeric_limits<int64_t>::max();
    auto           v     = radr::iota(first, last);
    std::vector    expected{first, first + 1};
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota, InfiniteRange)
{
    auto v  = radr::iota(0);
    auto it = v.begin();
    EXPECT_EQ(*it, 0);
    EXPECT_EQ(*(++it), 1);
    EXPECT_EQ(*(++it), 2);

    EXPECT_TRUE(std::ranges::input_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::sized_range<decltype(v)>);
    EXPECT_TRUE(std::ranges::random_access_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(v)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(v)>);
}

TEST(iota, IotaWithCharType)
{
    auto        v = radr::iota('a', 'f');
    std::vector expected{'a', 'b', 'c', 'd', 'e'};
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota, SizeIsCorrect)
{
    auto v = radr::iota(100, 110);
    EXPECT_EQ(std::ranges::size(v), 10);
}

TEST(iota, ConceptChecks)
{
    auto v = radr::iota(1, 4);

    EXPECT_TRUE(std::ranges::input_range<decltype(v)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(v)>);
    EXPECT_TRUE(std::ranges::random_access_range<decltype(v)>);
    EXPECT_TRUE(std::ranges::common_range<decltype(v)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(v)>);
}

TEST(iota, BoundedNonCommon)
{
    auto in = std::vector{1, 2, 3, 4, 5, 6} | radr::take_while([](int i) { return i < 5; });

    auto v = radr::iota(in.begin(), in.end()) | radr::transform([](auto it) { return *it; });

    EXPECT_RANGE_EQ(v, (std::vector{1, 2, 3, 4}));
    EXPECT_FALSE(std::ranges::sized_range<decltype(v)>);
    EXPECT_TRUE(std::ranges::random_access_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(v)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(v)>);
}

TEST(iota, BoundedNonCommon2)
{
    auto in = std::list{1, 2, 3, 4, 5, 6} | radr::take_while([](int i) { return i < 5; });

    auto v = radr::iota(in.begin(), in.end()) | radr::transform([](auto it) { return *it; });

    EXPECT_RANGE_EQ(v, (std::vector{1, 2, 3, 4}));
    EXPECT_FALSE(std::ranges::sized_range<decltype(v)>);
    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(v)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(v)>);
}

TEST(iota, IteratorOps)
{
    auto v = radr::iota(0, 100);

    auto it = v.begin();
    EXPECT_EQ(*it, 0);
    EXPECT_EQ(it[5], 5);
    EXPECT_EQ(it[42], 42);

    auto it2 = it + 19;
    EXPECT_EQ(*it2, 19);
    EXPECT_EQ(it2 - it, 19);
}

//===========================================================================
// single-pass version
//===========================================================================

// generator is not yet constexpr
TEST(iota_sp, Basic)
{
    auto        v = radr::iota_sp(0, 5);
    std::vector expected{0, 1, 2, 3, 4};
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota_sp, EmptyRange)
{
    auto             v = radr::iota_sp(10, 10);
    std::vector<int> expected;
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota_sp, SingleElement)
{
    auto        v = radr::iota_sp(42, 43);
    std::vector expected{42};
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota_sp, NegativeValues)
{
    auto        v = radr::iota_sp(-2, 2);
    std::vector expected{-2, -1, 0, 1};
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota_sp, LargeIntegers)
{
    constexpr auto first = std::numeric_limits<int64_t>::max() - 2;
    constexpr auto last  = std::numeric_limits<int64_t>::max();
    auto           v     = radr::iota_sp(first, last);
    std::vector    expected{first, first + 1};
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota_sp, InfiniteRange)
{
    auto v  = radr::iota_sp(0);
    auto it = v.begin();
    EXPECT_EQ(*it, 0);
    EXPECT_EQ(*(++it), 1);
    EXPECT_EQ(*(++it), 2);

    EXPECT_TRUE(std::ranges::input_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::sized_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::borrowed_range<decltype(v)>);
}

TEST(iota_sp, IotaWithCharType)
{
    auto        v = radr::iota_sp('a', 'f');
    std::vector expected{'a', 'b', 'c', 'd', 'e'};
    EXPECT_RANGE_EQ(v, expected);
}

TEST(iota_sp, BoundedNonCommon)
{
    auto in = std::vector{1, 2, 3, 4, 5, 6} | radr::take_while([](int i) { return i < 5; });

    auto v = radr::iota_sp(in.begin(), in.end()) | radr::transform([](auto it) { return *it; });

    EXPECT_RANGE_EQ(v, (std::vector{1, 2, 3, 4}));
    EXPECT_TRUE(std::ranges::input_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::sized_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::borrowed_range<decltype(v)>);
}

TEST(iota_sp, BoundedNonCommon2)
{
    auto in = std::list{1, 2, 3, 4, 5, 6} | radr::take_while([](int i) { return i < 5; });

    auto v = radr::iota_sp(in.begin(), in.end()) | radr::transform([](auto it) { return *it; });

    EXPECT_RANGE_EQ(v, (std::vector{1, 2, 3, 4}));
    EXPECT_TRUE(std::ranges::input_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::sized_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(v)>);
    EXPECT_FALSE(std::ranges::borrowed_range<decltype(v)>);
}
