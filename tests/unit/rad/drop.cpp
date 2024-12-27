#include <deque>
#include <forward_list>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/rad/drop.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

inline std::vector<size_t> const comp{3, 4, 5, 6};

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

TEST(drop, input)
{
    auto ra = radr::test::iota_input_range(1, 7) | radr::drop(2);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), radr::generator<size_t>);
}

// --------------------------------------------------------------------------
// forward tests
// --------------------------------------------------------------------------

template <typename _container_t>
struct drop_forward : public testing::Test
{
    /* data members */
    _container_t in{1, 2, 3, 4, 5, 6};

    /* type foo */
    using container_t = _container_t;

    using it_t   = radr::iterator_t<container_t>;
    using sen_t  = it_t;
    using cit_t  = radr::const_iterator_t<container_t>;
    using csen_t = cit_t;

    static constexpr radr::borrowing_rad_kind bk =
      std::ranges::sized_range<container_t> ? radr::borrowing_rad_kind::sized : radr::borrowing_rad_kind::unsized;
    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, bk>;

    template <typename in_t>
    static void type_checks()
    {
        radr::test::generic_adaptor_checks<in_t, container_t>();

        /* drop always results in the same iterators */
        EXPECT_TRUE((radr::detail::equivalent_range<in_t, container_t>));
        EXPECT_TRUE((radr::detail::equivalent_range<in_t const, container_t const>));
    }
};

//TODO: add non-common,
using container_types = ::testing::Types<std::forward_list<size_t>, // unsized
                                         std::list<size_t>,         // sized without sized_sentinel
                                         std::deque<size_t>,
                                         std::vector<size_t>>;

TYPED_TEST_SUITE(drop_forward, container_types);

TYPED_TEST(drop_forward, rvalue)
{
    auto ra = std::move(this->in) | radr::drop(2);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra),
                     (radr::owning_rad<typename TestFixture::borrow_t>));
    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(drop_forward, lvalue)
{
    auto ra = std::ref(this->in) | radr::drop(2);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), typename TestFixture::borrow_t);
    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(drop_forward, folding)
{
    auto ra = std::ref(this->in) | radr::drop(1) | radr::drop(1);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), typename TestFixture::borrow_t);
    TestFixture::template type_checks<decltype(ra)>();
}
