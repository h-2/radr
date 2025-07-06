#include <deque>
#include <forward_list>
#include <list>
#include <ranges>
#include <span>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#define RADR_ALL_NO_DEPRECATED 1
#include <radr/concepts.hpp>
#include <radr/rad/all.hpp>
#include <radr/range_access.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

inline std::vector<size_t> const comp{1, 2, 3, 4, 5, 6};

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

#if !defined(_GLIBCXX_RELEASE) || (_GLIBCXX_RELEASE >= 12)
TEST(all, input)
{
    std::istringstream input("10 20 30 40");

    auto rng = std::views::istream<int>(input) | radr::all;

    EXPECT_SAME_TYPE(decltype(rng), (std::ranges::basic_istream_view<int, char>));
}
#endif

// --------------------------------------------------------------------------
// forward tests
// --------------------------------------------------------------------------

template <typename _container_t>
struct all_forward : public testing::Test
{
    /* data members */
    _container_t in{1, 2, 3, 4, 5, 6};

    /* type foo */
    using container_t = _container_t;

    using it_t   = decltype(radr::begin(std::declval<container_t &>()));
    using sen_t  = decltype(radr::end(std::declval<container_t &>()));
    using cit_t  = decltype(radr::cbegin(std::declval<container_t const &>()));
    using csen_t = decltype(radr::cend(std::declval<container_t const &>()));

    static constexpr radr::borrowing_rad_kind bk =
      std::ranges::sized_range<container_t> ? radr::borrowing_rad_kind::sized : radr::borrowing_rad_kind::unsized;
    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, bk>;

    template <typename in_t>
    static void type_checks_impl()
    {
        /* preserved for all all adaptors */
        EXPECT_EQ(std::ranges::sized_range<in_t>, std::ranges::sized_range<container_t>);
        EXPECT_EQ(std::ranges::common_range<in_t>, std::ranges::common_range<container_t>);
        EXPECT_EQ(std::ranges::bidirectional_range<in_t>, std::ranges::bidirectional_range<container_t>);
        EXPECT_EQ(std::ranges::random_access_range<in_t>, std::ranges::random_access_range<container_t>);
        EXPECT_EQ(std::ranges::contiguous_range<in_t>, std::ranges::contiguous_range<container_t>);

        /* valid for all all adaptors */
        EXPECT_TRUE(radr::const_symmetric_range<in_t>);
    }

    template <typename in_t>
    static void type_checks()
    {
        radr::test::generic_adaptor_checks<in_t, container_t>();

        EXPECT_SAME_TYPE(radr::iterator_t<in_t>, it_t);
        EXPECT_SAME_TYPE(radr::sentinel_t<in_t>, sen_t);
        EXPECT_SAME_TYPE(radr::const_iterator_t<in_t>, cit_t);
        EXPECT_SAME_TYPE(radr::const_sentinel_t<in_t>, csen_t);
    }
};

//TODO: add non-common, add non-ref referenct_t
using container_types = ::testing::Types<std::forward_list<size_t>, // unsized
                                         std::list<size_t>,         // sized without sized_sentinel
                                         std::deque<size_t>,
                                         std::vector<size_t>>;

TYPED_TEST_SUITE(all_forward, container_types);

TYPED_TEST(all_forward, rvalue)
{
    using container_t = TestFixture::container_t;
    using borrow_t    = TestFixture::borrow_t;

    auto ra = std::move(this->in) | radr::all;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(all_forward, lvalue)
{
    using borrow_t = TestFixture::borrow_t;

    auto ra = std::ref(this->in) | radr::all;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), borrow_t);

    TestFixture::template type_checks<decltype(ra)>();
}

// --------------------------------------------------------------------------
// owning copy test
// --------------------------------------------------------------------------

TEST(all, owning_copy_test)
{
    auto own = std::vector{1, 2, 3, 4, 5, 6} | radr::all;
    EXPECT_RANGE_EQ(own, comp);

    auto cpy = own;
    EXPECT_RANGE_EQ(own, cpy);
}

#undef RADR_ALL_NO_DEPRECATED
