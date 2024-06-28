#include <forward_list>
#include <ranges>

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
// forward_list
// --------------------------------------------------------------------------

TEST(split, forward_range_elem)
{
    std::forward_list<int> l{1, 2, 3, 2, 4, 5, 2, 2, 2, 6, 2};

    auto ra = std::ref(l) | radr::split(2);

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, std::vector{1});
    ++it;
    EXPECT_RANGE_EQ(*it, std::vector{3});
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector{4, 5}));
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{}));
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{}));
    ++it;
    EXPECT_RANGE_EQ(*it, std::vector{6});
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{}));
    ++it;
    EXPECT_EQ(it, ra.end());

    // default init
    EXPECT_TRUE(std::ranges::empty(decltype(ra){}));
}

TEST(split, forward_range_pattern)
{
    std::forward_list<int> l{1, 2, 3, 2, 4, 5, 2, 3, 2, 6, 2};
    std::forward_list<int> p{2, 3};

    auto ra = std::ref(l) | radr::split(std::ref(p));

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, std::vector{1});
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector{2, 4, 5}));
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector{2, 6, 2}));
    ++it;
    EXPECT_EQ(it, ra.end());

    // default init
    EXPECT_TRUE(std::ranges::empty(decltype(ra){}));
}

// --------------------------------------------------------------------------
// string + Different pattern types
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

    // default init
    EXPECT_TRUE(std::ranges::empty(decltype(ra){}));
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

    // default init
    EXPECT_TRUE(std::ranges::empty(decltype(ra){}));
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

    // default init
    EXPECT_TRUE(std::ranges::empty(decltype(ra){}));
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

    // default init
    EXPECT_TRUE(std::ranges::empty(decltype(ra){}));
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

// --------------------------------------------------------------------------
// concepts
// --------------------------------------------------------------------------

template <typename ra_t>
void type_checks_impl()
{
    EXPECT_TRUE(std::ranges::forward_range<ra_t>);
    EXPECT_FALSE(std::ranges::bidirectional_range<ra_t>);
    EXPECT_FALSE(std::ranges::sized_range<ra_t>);
    EXPECT_FALSE(std::ranges::common_range<ra_t>);
}

TEST(split, concepts)
{
    {
        std::string s  = "thisXisXaXtest";
        auto        ra = std::ref(s) | radr::split('X');
        type_checks_impl<decltype(ra)>();
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, radr::borrowing_rad<char *>);
    }

    {
        std::string s  = "thisXisXaXtest";
        auto const  ra = std::ref(s) | radr::split('X');
        type_checks_impl<decltype(ra)>();
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, std::string_view);
    }

    {
        std::string const s  = "thisXisXaXtest";
        auto              ra = std::ref(s) | radr::split('X');
        type_checks_impl<decltype(ra)>();
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, std::string_view);
    }

    {
        std::string const s  = "thisXisXaXtest";
        auto const        ra = std::ref(s) | radr::split('X');
        type_checks_impl<decltype(ra)>();
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, std::string_view);
    }
}
