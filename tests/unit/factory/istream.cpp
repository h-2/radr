#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/istream.hpp>

TEST(ViewsIstreamConceptsTest, ConceptValidation)
{
    std::istringstream input("1 2 3");
    auto               view = radr::istream<int>(input);

    // Checking the relevant concepts
    EXPECT_TRUE(std::ranges::input_range<decltype(view)>);
    EXPECT_TRUE(std::ranges::view<decltype(view)>);
    EXPECT_TRUE(std::is_move_constructible_v<decltype(view)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(view)>);
    EXPECT_FALSE(std::ranges::sized_range<decltype(view)>);
    EXPECT_FALSE(std::ranges::contiguous_range<decltype(view)>);
    EXPECT_FALSE(std::is_default_constructible_v<decltype(view)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(view)>);
}

TEST(istream, EmptyInput)
{
    std::istringstream input("");
    auto               view = radr::istream<int>(input);
    EXPECT_TRUE(view.begin() == view.end());
}

TEST(istream, ReadIntegers)
{
    std::istringstream input("10 20 30");
    auto               view = radr::istream<int>(input);
    EXPECT_RANGE_EQ(view, (std::vector<int>{10, 20, 30}));
}

TEST(istream, ReadStrings)
{
    std::istringstream input("hello world test");
    auto               view = radr::istream<std::string>(input);
    EXPECT_RANGE_EQ(view, (std::vector<std::string>{"hello", "world", "test"}));
}

TEST(istream, StopOnMalformedData)
{
    std::istringstream input("42 invalid 84");
    auto               view = radr::istream<int>(input);
    EXPECT_RANGE_EQ(view, (std::vector<int>{42}));
}

TEST(istream, ReadChars)
{
    std::istringstream input("hello world test");
    auto               view = radr::istream<char>(input);
    EXPECT_RANGE_EQ(view, std::string{"helloworldtest"});
}
