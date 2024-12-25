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

#include <radr/concepts.hpp>
#include <radr/rad/as_const.hpp>

#include "radr/rad_util/borrowing_rad.hpp"
#include "radr/range_access.hpp"

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

    using it_t   = decltype(radr::cbegin(std::declval<container_t &>()));
    using sen_t  = decltype(radr::cend(std::declval<container_t &>()));
    using cit_t  = decltype(radr::cbegin(std::declval<container_t const &>()));
    using csen_t = decltype(radr::cend(std::declval<container_t const &>()));

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
        radr::test::generic_adaptor_checks<in_t, container_t>();

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

    auto ra = std::move(this->in) | radr::as_const;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(as_const_forward, lvalue)
{
    using borrow_t = TestFixture::borrow_t;

    static_assert(!radr::constant_range<typename TestFixture::container_t &>);
    static_assert(radr::constant_range<typename TestFixture::container_t const &>);
    auto ra = std::ref(this->in) | radr::as_const;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), borrow_t);

    TestFixture::template type_checks<decltype(ra)>();
}

// this tests radr::cbegin 's first case (Urange const != constant_range but range is contiguous_range)
TEST(as_const_forward2, span)
{
    std::vector<size_t> vec = comp;
    EXPECT_FALSE(radr::constant_range<decltype(vec)>);
    EXPECT_TRUE(radr::constant_range<decltype(std::as_const(vec))>);

    auto span = std::span{vec};
    EXPECT_FALSE(radr::constant_range<decltype(span)>);
    EXPECT_FALSE(radr::constant_range<decltype(std::as_const(span))>);

    auto ra1 = std::ref(span) | radr::as_const;
    EXPECT_TRUE(radr::constant_range<decltype(ra1)>);
    EXPECT_TRUE(radr::constant_range<decltype(std::as_const(ra1))>);
    EXPECT_SAME_TYPE(decltype(ra1), radr::borrowing_rad<size_t const *>);
    EXPECT_RANGE_EQ(ra1, comp);

    auto ra2 = std::move(span) | radr::as_const;
    EXPECT_TRUE(radr::constant_range<decltype(ra2)>);
    EXPECT_TRUE(radr::constant_range<decltype(std::as_const(ra2))>);
    EXPECT_SAME_TYPE(decltype(ra2), radr::borrowing_rad<size_t const *>);
    EXPECT_RANGE_EQ(ra2, comp);
}

// this tests radr::cbegin 's second case that uses basic_constant_iterator
TEST(as_const_forward2, subrange)
{
    using DIt = std::deque<size_t>::iterator;
    std::deque<size_t> vec;
    std::ranges::copy(comp, std::back_inserter(vec));
    EXPECT_FALSE(radr::constant_range<decltype(vec)>);
    EXPECT_TRUE(radr::constant_range<decltype(std::as_const(vec))>);

    /* subrange exposes deque::iterator also as its const_iterator and prevents access to deque::const_iterator */
    auto span = std::ranges::subrange{vec};
    EXPECT_FALSE(radr::constant_range<decltype(span)>);
    EXPECT_FALSE(radr::constant_range<decltype(std::as_const(span))>);
    EXPECT_SAME_TYPE(decltype(span), (std::ranges::subrange<DIt, DIt>));

    /* because we don't know about deque::const_iterator we create the const_iterator via basic_const_iterator */
    auto ra = std::move(span) | radr::as_const;
    EXPECT_FALSE(radr::constant_range<decltype(vec)>);
    EXPECT_TRUE(radr::constant_range<decltype(std::as_const(vec))>);
    using RaDit = radr::detail::basic_const_iterator<DIt>;
    EXPECT_SAME_TYPE(decltype(ra), (radr::borrowing_rad<RaDit, RaDit, RaDit, RaDit>));
    EXPECT_RANGE_EQ(ra, comp);
}

TEST(as_const_forward2, cref_ours)
{
    std::vector<size_t> vec = comp;

    radr::borrowing_rad ra0{vec};

    auto ra1 = std::cref(ra0) | radr::as_const;
    EXPECT_SAME_TYPE(decltype(ra1), radr::borrowing_rad<size_t const *>);
    EXPECT_RANGE_EQ(ra1, comp);
}

TEST(as_const_forward2, string)
{
    std::string str = "foobar";

    auto ra0 = std::ref(str) | radr::as_const;
    EXPECT_SAME_TYPE(decltype(ra0), std::string_view);
    EXPECT_RANGE_EQ(ra0, str);

    auto ra1 = std::cref(str) | radr::as_const;
    EXPECT_SAME_TYPE(decltype(ra1), std::string_view);
    EXPECT_RANGE_EQ(ra1, str);
}

// --------------------------------------------------------------------------
// owning copy test
// --------------------------------------------------------------------------

TEST(as_const, owning_copy_test)
{
    auto own = std::vector{1, 2, 3, 4, 5, 6} | radr::as_const;
    EXPECT_RANGE_EQ(own, comp);

    auto cpy = own;
    EXPECT_RANGE_EQ(own, cpy);
}
