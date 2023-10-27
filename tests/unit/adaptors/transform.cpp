#include <deque>
#include <forward_list>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/gtest_helpers.hpp>

#include <radr/transform.hpp>

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

radr::generator<size_t> iota_input_range(size_t const b, size_t const e)
{
    for (size_t i = b; i < e; ++i)
        co_yield i;
}

TEST(transform, input)
{
    auto plus1 = [](size_t const i) { return i + 1; };

    auto ra = iota_input_range(1, 7) | radr::pipe::transform(plus1);

    EXPECT_RANGE_EQ(ra, (std::vector<size_t>{2, 3, 4, 5, 6, 7}));
    EXPECT_SAME_TYPE(decltype(ra), radr::generator<size_t>);
}

// --------------------------------------------------------------------------
// forwrd test
// --------------------------------------------------------------------------

template <typename _container_t>
struct transform_forward : public testing::Test
{
    using container_t = _container_t;

    static constexpr auto plus1 = [](size_t const i) { return i + 1; };

    using it_t   = radr::transform_iterator<container_t, std::remove_cvref_t<decltype(plus1)>, false>;
    using sen_t  = it_t;
    using cit_t  = radr::transform_iterator<container_t, std::remove_cvref_t<decltype(plus1)>, true>;
    using csen_t = cit_t;

    static constexpr radr::borrowing_rad_kind bk =
      std::ranges::sized_range<container_t> ? radr::borrowing_rad_kind::sized : radr::borrowing_rad_kind::unsized;
    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, bk>;

    template <typename in_t>
    static void type_checks()
    {
        /* for all our forward range adaptors */
        EXPECT_TRUE(radr::const_iterable_range<in_t>);
        EXPECT_TRUE(std::regular<in_t>);

        /* preserved for all transform adaptors */
        EXPECT_EQ(std::ranges::sized_range<in_t>, std::ranges::sized_range<container_t>);
        EXPECT_EQ(std::ranges::common_range<in_t>, std::ranges::common_range<container_t>);
        EXPECT_EQ(std::ranges::bidirectional_range<in_t>, std::ranges::bidirectional_range<container_t>);
        EXPECT_EQ(std::ranges::random_access_range<in_t>, std::ranges::random_access_range<container_t>);

        /* never preserved for transform adaptors */
        EXPECT_FALSE(std::ranges::contiguous_range<in_t>);
    }
};

//TODO: add non-common
using container_types = ::testing::Types<std::forward_list<size_t>, // unsized
                                         std::list<size_t>,         // sized without sized_sentinel
                                         std::deque<size_t>,
                                         std::vector<size_t>>;

TYPED_TEST_SUITE(transform_forward, container_types);

TYPED_TEST(transform_forward, rvalue)
{
    using container_t = TestFixture::container_t;
    using borrow_t    = TestFixture::borrow_t;

    container_t in{1, 2, 3, 4, 5, 6};
    auto        ra = std::move(in) | radr::pipe::transform(this->plus1);

    EXPECT_RANGE_EQ(ra, (std::vector<size_t>{2, 3, 4, 5, 6, 7}));
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(transform_forward, lvalue)
{
    using container_t = TestFixture::container_t;
    // using borrow_t    = TestFixture::borrow_t;

    container_t in{1, 2, 3, 4, 5, 6};
    auto        ra = std::ref(in) | radr::pipe::transform(this->plus1);

    EXPECT_RANGE_EQ(ra, (std::vector<size_t>{2, 3, 4, 5, 6, 7}));
    // EXPECT_SAME_TYPE(decltype(ra), borrow_t); // see radr-internal#1

    TestFixture::template type_checks<decltype(ra)>();
}
