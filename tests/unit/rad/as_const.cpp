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
#include <radr/rad/as_const.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

inline std::vector<size_t> const comp{1, 2, 3, 4, 5, 6};

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

/* does not operate in single-pass ranges */

// --------------------------------------------------------------------------
// forward tests
// --------------------------------------------------------------------------

template <typename _container_t>
struct as_const_forward : public testing::Test
{
    /* data members */
    _container_t in{1, 2, 3, 4, 5, 6};

    /* type foo */
    using container_t = _container_t;

    using it_t   = decltype(std::ranges::cbegin(std::declval<container_t &>()));
    using sen_t  = decltype(std::ranges::cend(std::declval<container_t &>()));
    using cit_t  = decltype(std::ranges::cbegin(std::declval<container_t const &>()));
    using csen_t = decltype(std::ranges::cend(std::declval<container_t const &>()));

    static constexpr radr::borrowing_rad_kind bk =
      std::ranges::sized_range<container_t> ? radr::borrowing_rad_kind::sized : radr::borrowing_rad_kind::unsized;
    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, bk>;

    template <typename in_t>
    static void type_checks_impl()
    {
        /* preserved for all as_const adaptors */
        EXPECT_EQ(std::ranges::sized_range<in_t>, std::ranges::sized_range<container_t>);
        EXPECT_EQ(std::ranges::common_range<in_t>, std::ranges::common_range<container_t>);
        EXPECT_EQ(std::ranges::bidirectional_range<in_t>, std::ranges::bidirectional_range<container_t>);
        EXPECT_EQ(std::ranges::random_access_range<in_t>, std::ranges::random_access_range<container_t>);
        EXPECT_EQ(std::ranges::contiguous_range<in_t>, std::ranges::contiguous_range<container_t>);

        /* valid for all as_const adaptors */
        EXPECT_TRUE(radr::const_symmetric_range<in_t>);
    }

    template <typename in_t>
    static void type_checks()
    {
        radr::test::generic_adaptor_checks<in_t, in_t const, container_t>();

        type_checks_impl<in_t>();
        type_checks_impl<in_t const>();

        EXPECT_SAME_TYPE(std::ranges::range_reference_t<in_t>, size_t const &);
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<in_t const>, size_t const &);
    }
};

//TODO: add non-common, add non-ref referenct_t
using container_types = ::testing::Types<std::forward_list<size_t>, // unsized
                                         std::list<size_t>,         // sized without sized_sentinel
                                         std::deque<size_t>,
                                         std::vector<size_t>>;

TYPED_TEST_SUITE(as_const_forward, container_types);

TYPED_TEST(as_const_forward, rvalue)
{
    using container_t = TestFixture::container_t;
    using borrow_t    = TestFixture::borrow_t;

    auto ra = std::move(this->in) | radr::pipe::as_const;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(as_const_forward, lvalue)
{
    // using borrow_t    = TestFixture::borrow_t;

    auto ra = std::ref(this->in) | radr::pipe::as_const;

    EXPECT_RANGE_EQ(ra, comp);
    // EXPECT_SAME_TYPE(decltype(ra), borrow_t); // see radr-internal#1

    TestFixture::template type_checks<decltype(ra)>();
}
