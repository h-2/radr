#include <concepts>
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
#include <radr/transform.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

inline constexpr auto            fn = [](size_t const i) { return i + 1; };
inline std::vector<size_t> const comp{2, 3, 4, 5, 6, 7};

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

TEST(transform, input)
{
    auto ra = radr::test::iota_input_range(1, 7) | radr::pipe::transform(fn);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), radr::generator<size_t>);
}

// --------------------------------------------------------------------------
// forward tests
// --------------------------------------------------------------------------

template <typename _container_t>
struct transform_forward : public testing::Test
{
    /* data members */
    _container_t in{1, 2, 3, 4, 5, 6};

    /* type foo */
    using container_t = _container_t;

    using it_t   = radr::transform_iterator<container_t, std::remove_cvref_t<decltype(fn)>, false>;
    using sen_t  = it_t;
    using cit_t  = radr::transform_iterator<container_t, std::remove_cvref_t<decltype(fn)>, true>;
    using csen_t = cit_t;

    static constexpr radr::borrowing_rad_kind bk =
      std::ranges::sized_range<container_t> ? radr::borrowing_rad_kind::sized : radr::borrowing_rad_kind::unsized;
    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, bk>;

    template <typename in_t>
    static void type_checks_impl()
    {
        /* preserved for all transform adaptors */
        EXPECT_EQ(std::ranges::sized_range<in_t>, std::ranges::sized_range<container_t>);
        EXPECT_EQ(std::ranges::common_range<in_t>, std::ranges::common_range<container_t>);
        EXPECT_EQ(std::ranges::bidirectional_range<in_t>, std::ranges::bidirectional_range<container_t>);
        EXPECT_EQ(std::ranges::random_access_range<in_t>, std::ranges::random_access_range<container_t>);

        /* never preserved for transform adaptors */
        EXPECT_FALSE(std::ranges::contiguous_range<in_t>);
    }

    template <typename in_t>
    static void type_checks()
    {
        radr::test::generic_adaptor_checks<in_t, in_t const, container_t>();

        type_checks_impl<in_t>();
        type_checks_impl<in_t const>();

        EXPECT_SAME_TYPE(std::ranges::range_reference_t<in_t>, size_t);
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<in_t const>, size_t);
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

    auto ra = std::move(this->in) | radr::pipe::transform(fn);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(transform_forward, rvalue_function_syntax)
{
    using container_t = TestFixture::container_t;
    using borrow_t    = TestFixture::borrow_t;

    auto ra = radr::pipe::transform(fn)(std::move(this->in));

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(transform_forward, rvalue_function_syntax2)
{
    using container_t = TestFixture::container_t;
    using borrow_t    = TestFixture::borrow_t;

    auto ra = radr::pipe::transform(std::move(this->in), fn);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(transform_forward, lvalue)
{
    // using borrow_t    = TestFixture::borrow_t;

    auto ra = std::ref(this->in) | radr::pipe::transform(fn);

    EXPECT_RANGE_EQ(ra, comp);
    // EXPECT_SAME_TYPE(decltype(ra), borrow_t); // see radr-internal#1

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(transform_forward, lvalue_function_syntax)
{
    // using borrow_t    = TestFixture::borrow_t;

    auto ra = radr::pipe::transform(fn)(std::ref(this->in));

    EXPECT_RANGE_EQ(ra, comp);
    // EXPECT_SAME_TYPE(decltype(ra), borrow_t); // see radr-internal#1

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(transform_forward, lvalue_function_syntax2)
{
    // using borrow_t    = TestFixture::borrow_t;

    auto ra = radr::pipe::transform(std::ref(this->in), fn);

    EXPECT_RANGE_EQ(ra, comp);
    // EXPECT_SAME_TYPE(decltype(ra), borrow_t); // see radr-internal#1

    TestFixture::template type_checks<decltype(ra)>();
}
