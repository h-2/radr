#include <forward_list>
#include <list>
#include <ranges>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/rad/join.hpp>
#include <radr/rad/to_single_pass.hpp>

using namespace std::string_view_literals;

// --------------------------------------------------------------------------
// single_pass test
// --------------------------------------------------------------------------

TEST(join, single_pass)
{
    auto ra = std::vector<std::string>{"foo", "" /*empty*/, "bar", "b"} | radr::to_single_pass | radr::join;

    EXPECT_RANGE_EQ(ra, "foobarb"sv);
    EXPECT_SAME_TYPE(decltype(ra), (radr::generator<char &, char>));
}

// --------------------------------------------------------------------------
// forward_list
// --------------------------------------------------------------------------

TEST(join, forward_range_concepts)
{
    std::forward_list<std::forward_list<char>> l{
      {'f', 'o', 'o'},
      {'b', 'a', 'r'},
      {'b'}
    };

    auto ra = std::ref(l) | radr::join;
    EXPECT_TRUE(std::ranges::forward_range<decltype(ra)>);
    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(ra)>);
    radr::test::generic_adaptor_checks<decltype(ra), decltype(l)>();
}

TEST(join, forward_range_default_empty)
{
    std::forward_list<std::forward_list<char>> l{};

    auto ra = std::ref(l) | radr::join;
    EXPECT_TRUE(std::ranges::empty(decltype(ra){}));
}

TEST(join, forward_range_easy)
{
    std::forward_list<std::forward_list<char>> l{
      {'f', 'o', 'o'},
      {'b', 'a', 'r'},
      {'b'}
    };

    auto ra = std::ref(l) | radr::join;
    EXPECT_RANGE_EQ(ra, "foobarb"sv);
}

TEST(join, forward_range_empties)
{
    std::forward_list<std::forward_list<char>> l{
      {},
      {'f', 'o', 'o'},
      {},
      {'b', 'a', 'r'},
      {'b'},
      {}
    };

    auto ra = std::ref(l) | radr::join;

    EXPECT_RANGE_EQ(ra, "foobarb"sv);
}

// --------------------------------------------------------------------------
// bidirectional_range
// --------------------------------------------------------------------------

TEST(join, bidi_range_default_empty)
{
    std::list<std::list<char>> l{};

    auto ra = std::ref(l) | radr::join;
    EXPECT_TRUE(std::ranges::empty(decltype(ra){}));
}

TEST(join, bidi_range_easy)
{
    std::list<std::list<char>> l{
      {'f', 'o', 'o'},
      {'b', 'a', 'r'},
      {'b'}
    };

    auto ra = std::ref(l) | radr::join;
    EXPECT_RANGE_EQ(ra, "foobarb"sv);
}

TEST(join, bidi_range_empties)
{
    std::list<std::list<char>> l{
      {},
      {'f', 'o', 'o'},
      {},
      {'b', 'a', 'r'},
      {'b'},
      {}
    };

    auto ra = std::ref(l) | radr::join;

    EXPECT_RANGE_EQ(ra, "foobarb"sv);
}

TEST(join, bidi_range_concepts)
{
    std::list<std::list<char>> l{
      {'f', 'o', 'o'},
      {'b', 'a', 'r'},
      {'b'}
    };

    auto ra = std::ref(l) | radr::join;
    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(ra)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(ra)>);
    radr::test::generic_adaptor_checks<decltype(ra), decltype(l)>();
}

TEST(join, bidi_range_iterate)
{
    std::list<std::list<char>> l{
      {},
      {'f', 'o', 'o'},
      {},
      {'b', 'a', 'r'},
      {'b'},
      {}
    };

    auto ra = std::ref(l) | radr::join;

    auto it = ra.end();
    --it;
    EXPECT_EQ(*it, 'b');
    --it;
    EXPECT_EQ(*it, 'r');
    --it;
    EXPECT_EQ(*it, 'a');
    --it;
    EXPECT_EQ(*it, 'b');
    --it;
    EXPECT_EQ(*it, 'o');
    --it;
    EXPECT_EQ(*it, 'o');
    --it;
    EXPECT_EQ(*it, 'f');

    EXPECT_EQ(it, ra.begin());
}

TEST(join, bidi_range_reverse)
{
    std::list<std::list<char>> l{
      {},
      {'f', 'o', 'o'},
      {},
      {'b', 'a', 'r'},
      {'b'},
      {}
    };

    auto ra = std::ref(l) | radr::join | std::views::reverse;
    EXPECT_RANGE_EQ(ra, "braboof"sv);
}
