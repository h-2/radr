#include <deque>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/rad/reverse.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

inline std::vector<size_t> const comp{6, 5, 4, 3, 2, 1};

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

/* does not support single pass ranges */

// --------------------------------------------------------------------------
// forward test
// --------------------------------------------------------------------------

template <typename _container_t>
struct reverse_forward : public testing::Test
{
    /* data members */
    _container_t in{1, 2, 3, 4, 5, 6};

    /* type foo */
    using container_t = _container_t;

    using it_t   = container_t::reverse_iterator;
    using sen_t  = it_t;
    using cit_t  = container_t::const_reverse_iterator;
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

        /* preserved for all reverse adaptors */
        EXPECT_EQ(std::ranges::sized_range<in_t>, std::ranges::sized_range<container_t>);
        EXPECT_EQ(std::ranges::common_range<in_t>, std::ranges::common_range<container_t>);
        EXPECT_EQ(std::ranges::bidirectional_range<in_t>, std::ranges::bidirectional_range<container_t>);
        EXPECT_EQ(std::ranges::random_access_range<in_t>, std::ranges::random_access_range<container_t>);

        /* never preserved for reverse adaptors */
        EXPECT_FALSE(std::ranges::contiguous_range<in_t>);

        /* associated types */
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<in_t>, size_t &);
    }
};

//TODO: add non-common, non-sized and linear-construction time example
using container_types = ::testing::Types</*std::forward_list<size_t>,*/ // unsized
                                         std::list<size_t>,             // sized without sized_sentinel
                                         std::deque<size_t>,
                                         std::vector<size_t>>;

TYPED_TEST_SUITE(reverse_forward, container_types);

TYPED_TEST(reverse_forward, rvalue)
{
    using container_t = TestFixture::container_t;
    using borrow_t    = TestFixture::borrow_t;

    auto ra = std::move(this->in) | radr::reverse;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(reverse_forward, lvalue)
{
    // using borrow_t    = TestFixture::borrow_t;

    auto ra = std::ref(this->in) | radr::reverse;

    EXPECT_RANGE_EQ(ra, comp);
    // EXPECT_SAME_TYPE(decltype(ra), borrow_t); // see radr-internal#1

    TestFixture::template type_checks<decltype(ra)>();
}
