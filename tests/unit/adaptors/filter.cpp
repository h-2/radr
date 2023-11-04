#include <deque>
#include <forward_list>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/rad/filter.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

constexpr auto                   fn = [](size_t const i) { return i % 2 == 0; };
inline std::vector<size_t> const comp{2, 4, 6};

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

TEST(filter, input)
{
    auto ra = radr::test::iota_input_range(1, 7) | radr::pipe::filter(fn);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), radr::generator<size_t>);
}

// --------------------------------------------------------------------------
// forward tests
// --------------------------------------------------------------------------

template <typename _container_t>
struct filter_forward : public testing::Test
{
    /* data members */
    _container_t in{1, 2, 3, 4, 5, 6};

    /* type foo */
    using container_t = _container_t;

    using it_t   = radr::filter_iterator<container_t, false, std::remove_cvref_t<decltype(fn)>>;
    using sen_t  = it_t;
    using cit_t  = radr::filter_iterator<container_t, true, std::remove_cvref_t<decltype(fn)>>;
    using csen_t = cit_t;

    static constexpr radr::borrowing_rad_kind bk = radr::borrowing_rad_kind::unsized;
    using borrow_t                               = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, bk>;

    template <typename in_t>
    static void type_checks_impl()
    {
        /* preserved for all filter adaptors */
        EXPECT_EQ(std::ranges::common_range<in_t>, std::ranges::common_range<container_t>);
        EXPECT_EQ(std::ranges::bidirectional_range<in_t>, std::ranges::bidirectional_range<container_t>);

        /* never preserved for filter adaptors */
        EXPECT_FALSE(std::ranges::sized_range<in_t>);
        EXPECT_FALSE(std::ranges::random_access_range<in_t>);
        EXPECT_FALSE(std::ranges::contiguous_range<in_t>);
    }

    template <typename in_t>
    static void type_checks()
    {
        radr::test::generic_adaptor_checks<in_t, in_t const, container_t>();

        type_checks_impl<in_t>();
        type_checks_impl<in_t const>();

        EXPECT_SAME_TYPE(std::ranges::range_reference_t<in_t>, size_t &);
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<in_t const>, size_t const &);
    }
};

//TODO: add non-common,
using container_types = ::testing::Types<std::forward_list<size_t>, // unsized
                                         std::list<size_t>,         // sized without sized_sentinel
                                         std::deque<size_t>,
                                         std::vector<size_t>>;

TYPED_TEST_SUITE(filter_forward, container_types);

TYPED_TEST(filter_forward, rvalue)
{
    using container_t = TestFixture::container_t;
    using borrow_t    = TestFixture::borrow_t;

    auto ra = std::move(this->in) | radr::pipe::filter(fn);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(filter_forward, lvalue)
{
    // using borrow_t    = TestFixture::borrow_t;
    auto ra = radr::pipe::filter(std::ref(this->in), fn);

    static_assert(std::ranges::input_range<decltype(ra)>);

    EXPECT_RANGE_EQ(ra, comp);
    // EXPECT_SAME_TYPE(decltype(ra), borrow_t); // see radr-internal#1

    TestFixture::template type_checks<decltype(ra)>();
}
