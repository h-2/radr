#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/istream.hpp>
#include <radr/rad/take.hpp>

TEST(ViewsIstreamConceptsTest, ConceptValidation)
{
    std::istringstream input("1 2 3");
    auto               rng = radr::istream<int>(input);

    // Checking the relevant concepts
    EXPECT_TRUE(std::ranges::input_range<decltype(rng)>);
    EXPECT_TRUE(std::ranges::view<decltype(rng)>);
    EXPECT_TRUE(std::is_move_constructible_v<decltype(rng)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(rng)>);
    EXPECT_FALSE(std::ranges::sized_range<decltype(rng)>);
    EXPECT_FALSE(std::ranges::contiguous_range<decltype(rng)>);
    EXPECT_FALSE(std::is_default_constructible_v<decltype(rng)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(rng)>);
}

TEST(istream, EmptyInput)
{
    std::istringstream input("");
    auto               rng = radr::istream<int>(input);
    EXPECT_TRUE(rng.begin() == rng.end());
}

TEST(istream, ReadIntegers)
{
    std::istringstream input("10 20 30");
    auto               rng = radr::istream<int>(input);
    EXPECT_RANGE_EQ(rng, (std::vector<int>{10, 20, 30}));
}

TEST(istream, ReadStrings)
{
    std::istringstream input("hello world test");
    auto               rng = radr::istream<std::string>(input);
    EXPECT_RANGE_EQ(rng, (std::vector<std::string>{"hello", "world", "test"}));
}

TEST(istream, StopOnMalformedData)
{
    std::istringstream input("42 invalid 84");
    auto               rng = radr::istream<int>(input);
    EXPECT_RANGE_EQ(rng, (std::vector<int>{42}));
}

TEST(istream, ReadChars)
{
    std::istringstream input("hello world test");
    auto               rng = radr::istream<char>(input);
    EXPECT_RANGE_EQ(rng, std::string{"helloworldtest"});
}

TEST(istream, NoReadBehind)
{
    {
        std::istringstream input("10 20 30 40");

        auto rng = radr::istream<int>(input) | radr::take(2);
        EXPECT_RANGE_EQ(rng, (std::vector<int>{10, 20}));

        int i = 0;
        input >> i;
        EXPECT_EQ(i, 30);
        // NO ELEMENT HAS BEEN SKIPPED
    }

#if !defined(_GLIBCXX_RELEASE) || (_GLIBCXX_RELEASE >= 12)
    {
        std::istringstream input("10 20 30 40");

        auto rng = std::views::istream<int>(input) | std::views::take(2);
        EXPECT_RANGE_EQ(rng, (std::vector<int>{10, 20}));

        int i = 0;
        input >> i;
        EXPECT_EQ(i, 40);
        // ELEMENT HAS BEEN SKIPPED
    }
#endif
}
