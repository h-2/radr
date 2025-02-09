#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/concepts.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/take.hpp>
#include <radr/rad/to_common.hpp>

TEST(iterator_size, non_prop)
{
    size_t count = 0;
    auto   even  = [&count](size_t i)
    {
        ++count;
        return i % 2 == 0;
    };

    std::list<size_t> vec{1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1};

    {
        auto v = vec | std::views::filter(even);
        for ([[maybe_unused]] size_t i : v)
        {
        }
        EXPECT_EQ(count, 15);
        count = 0;

        for ([[maybe_unused]] size_t i : v)
        {
        }
        EXPECT_EQ(count, 9);
        count = 0;

        auto v2 = v | std::views::take(100);
        for ([[maybe_unused]] size_t i : v2)
        {
        }
        EXPECT_EQ(count, 15);
        count = 0;
    }

    {
        auto v = std::ref(vec) | radr::filter(even);
        for ([[maybe_unused]] size_t i : v)
        {
        }
        EXPECT_EQ(count, 15);
        count = 0;

        for ([[maybe_unused]] size_t i : v)
        {
        }
        EXPECT_EQ(count, 9);
        count = 0;

        auto v2 = v | radr::take(100);
        for ([[maybe_unused]] size_t i : v2)
        {
        }
        EXPECT_EQ(count, 9);
        count = 0;

        auto v3 = std::move(v) | std::views::take(100);
        for ([[maybe_unused]] size_t i : v3)
        {
        }
        EXPECT_EQ(count, 9);
        count = 0;
    }
}
