#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/rad/make_single_pass.hpp>
#include <radr/rad/split.hpp>

using namespace std::string_view_literals;

// --------------------------------------------------------------------------
// single_pass test
// --------------------------------------------------------------------------

TEST(split, single_pass)
{
    auto ra = std::string("thisXisXaXtest") | radr::make_single_pass | radr::split('X');

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "this"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "is"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "a"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "test"sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), (radr::generator<char &, char> &));
}

TEST(split, single_pass_empty)
{
    auto ra = std::string("thisXisXXaXtest") | radr::make_single_pass | radr::split('X');

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "this"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "is"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, ""sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "a"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "test"sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), (radr::generator<char &, char> &));
}

TEST(split, single_pass_trailing_empty)
{
    auto ra = std::string("thisXisXaXtestX") | radr::make_single_pass | radr::split('X');

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "this"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "is"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "a"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "test"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, ""sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), (radr::generator<char &, char> &));
}

// --------------------------------------------------------------------------
// Different pattern types
// --------------------------------------------------------------------------

TEST(split, lvalue_range_pattern)
{
    std::string const s       = "thisfooisfooafootest";
    std::string const pattern = "foo";
    auto              ra      = std::ref(s) | radr::split(std::ref(pattern));

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "this"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "is"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "a"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "test"sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), std::string_view);
}

TEST(split, rvalue_range_pattern)
{
    std::string const s  = "thisfooisfooafootest";
    auto              ra = std::ref(s) | radr::split("foo"sv);

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "this"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "is"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "a"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "test"sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), std::string_view);
}

// Not supported at the moment
// TEST(split, cstring_literal_pattern)
// {
//     std::string const s       = "thisfooisfooafootest";
//     auto              ra      = std::ref(s) | radr::split("foo");
//
//     auto it = ra.begin();
//     EXPECT_RANGE_EQ(*it, "this"sv);
//     ++it;
//     EXPECT_RANGE_EQ(*it, "is"sv);
//     ++it;
//     EXPECT_RANGE_EQ(*it, "a"sv);
//     ++it;
//     EXPECT_RANGE_EQ(*it, "test"sv);
//     ++it;
//     EXPECT_EQ(it, ra.end());
//
//     EXPECT_SAME_TYPE(decltype(*it), std::string_view);
// }

TEST(split, lvalue_element_pattern)
{
    std::string const s       = "thisXisXaXtest";
    char              pattern = 'X';
    auto              ra      = std::ref(s) | radr::split(std::ref(pattern));

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "this"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "is"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "a"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "test"sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), std::string_view);
}

TEST(split, rvalue_element_pattern)
{
    std::string const s  = "thisXisXaXtest";
    auto              ra = std::ref(s) | radr::split('X');

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "this"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "is"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "a"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "test"sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), std::string_view);
}

// --------------------------------------------------------------------------
// empty matches
// --------------------------------------------------------------------------

TEST(split, empty)
{
    std::string const s       = "thisfooisfoofooafootest";
    std::string_view  pattern = "foo";
    auto              ra      = std::ref(s) | radr::split(pattern);

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "this"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "is"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, ""sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "a"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "test"sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), std::string_view);
}

TEST(split, empty_end)
{
    std::string const s       = "thisfooisfooafootestfoo";
    std::string_view  pattern = "foo";
    auto              ra      = std::ref(s) | radr::split(pattern);

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "this"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "is"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "a"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "test"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, ""sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), std::string_view);
}
