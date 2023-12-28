#include <deque>
#include <forward_list>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/concepts.hpp>
#include <radr/rad/make_single_pass.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

inline std::vector<size_t> const comp{1, 2, 3, 4, 5, 6};

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

TEST(transform, input)
{
    auto ra = radr::test::iota_input_range(1, 7) | radr::pipe::make_single_pass; // NOOP

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), radr::generator<size_t>);
}

TEST(transform, std_views_istream)
{
    auto ra = std::ranges::istream_view<char>(std::cin) | radr::pipe::make_single_pass; // NOOP

    EXPECT_SAME_TYPE(decltype(ra), (std::ranges::basic_istream_view<char, char>));
}

// --------------------------------------------------------------------------
// "forward" tests
// --------------------------------------------------------------------------

template <typename _container_t>
struct make_single_pass_forward : public testing::Test
{
    /* data members */
    _container_t in{1, 2, 3, 4, 5, 6};
};

//TODO: add non-common, add non-ref referenct_t
using container_types = ::testing::Types<std::forward_list<size_t>, // unsized
                                         std::list<size_t>,         // sized without sized_sentinel
                                         std::deque<size_t>,
                                         std::vector<size_t>>;

TYPED_TEST_SUITE(make_single_pass_forward, container_types);

TYPED_TEST(make_single_pass_forward, rvalue)
{
    auto ra = std::move(this->in) | radr::pipe::make_single_pass;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::generator<size_t &, size_t>));
}

TYPED_TEST(make_single_pass_forward, lvalue)
{
    auto ra = std::ref(this->in) | radr::pipe::make_single_pass;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::generator<size_t &, size_t>));
}
