#include <deque>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/rad/cache_end.hpp>
#include <radr/rad/filter.hpp>

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

/* does not support single pass ranges */

// --------------------------------------------------------------------------
// forward test
// --------------------------------------------------------------------------

TEST(cache_end, noop)
{
    std::vector<int> v{1, 2, 3, 4, 5};

    auto ra = std::ref(v) | radr::cache_end;

    EXPECT_SAME_TYPE(decltype(ra), radr::borrowing_rad<int *>);
    EXPECT_EQ(radr::begin(v), radr::begin(ra));
    EXPECT_EQ(radr::end(v), radr::end(ra));
}

TEST(cache_end, filter)
{
    std::vector<int> v{1, 2, 3, 3, 3, 2, 1};

    auto ra = std::ref(v) | radr::filter([](int i) { return i == 3; });

    EXPECT_FALSE(radr::common_range<decltype(ra)>);
    EXPECT_FALSE(std::ranges::sized_range<decltype(ra)>);
    EXPECT_EQ(std::ranges::distance(ra), 3);

    auto ra2 = std::ref(ra) | radr::cache_end;

    EXPECT_TRUE(radr::common_range<decltype(ra2)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(ra2)>);
    EXPECT_EQ(std::ranges::size(ra2), 3);
}
