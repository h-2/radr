#include <cctype>
#include <ranges>
#include <string>
#include <type_traits>

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
             | radr::transform(to_upper)                   // "ISA"
             | radr::drop(1);                              // "SA"

    EXPECT_RANGE_EQ(r, "SA");

    auto copy = r;
    EXPECT_RANGE_EQ(copy, "SA");
}

#if RADR_FEATURE_ZIP

#    include <radr/rad/elements.hpp>
#    include <radr/rad/enumerate.hpp>

TEST(owning_copy, zip)
{
    constexpr auto sub = [](std::string_view str)
    {
        return str.substr(0, 1);
    };
#    define RADR_EXPR                                                                                                  \
        std::vector<std::string>{"foo", "bar", "baz"} | radr::transform(sub) | radr::enumerate | radr::elements<1>

    std::vector<std::string> compi{"f", "b", "b"};
    using T = std::remove_cvref_t<decltype(RADR_EXPR)>;
    T cpy;
    {
        T own = RADR_EXPR;
        EXPECT_RANGE_EQ(own, compi);
        cpy = own;
    }
    EXPECT_RANGE_EQ(cpy, compi);
}

#endif
