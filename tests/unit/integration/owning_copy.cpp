#include <cctype>
#include <ranges>
#include <string>

#include <gtest/gtest.h>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/concepts.hpp>
#include <radr/rad/drop.hpp>
#include <radr/rad/drop_while.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/join.hpp>
#include <radr/rad/split.hpp>
#include <radr/rad/take.hpp>
#include <radr/rad/transform.hpp>

TEST(owning_copy, owning_copy)
{
    std::string input = "thisXisXaXtestXwithXsomeXxses";

    auto is_lower = [](unsigned char c)
    {
        return std::islower(c);
    };
    auto to_upper = [](unsigned char c) -> char
    {
        return std::toupper(c);
    };

    auto r = std::move(input) | radr::drop_while(is_lower) // "XisXaXtestXwithXsomeXxses"
             | radr::split('X')                            // "" "is" "a" "test" "with" "some" "xses"
             | radr::as_const                              // ...
             | radr::take(3)                               // "" "is" "a"
             | radr::join                                  // "isa"
             | radr::transform(to_upper);                   // "ISA"
             // | radr::drop(1);                              // "SA"

    EXPECT_RANGE_EQ(r, "SA");

    // auto copy = r;
    // EXPECT_RANGE_EQ(copy, "SA");
}
