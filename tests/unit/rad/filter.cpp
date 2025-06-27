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
#include <radr/detail/detail.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/take.hpp>
#include <radr/rad/to_common.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

constexpr auto fn = [](size_t const i)
{
    return i % 2 == 0;
};
inline std::vector<size_t> const comp{2, 4, 6};

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

TEST(filter, input)
{
    auto ra = radr::test::iota_input_range(1, 7) | radr::filter(fn);

    EXPECT_RANGE_EQ(ra, comp);
#ifdef __cpp_lib_generator
    EXPECT_SAME_TYPE(decltype(ra), (radr::generator<size_t &&, size_t>));
#else
    EXPECT_SAME_TYPE(decltype(ra), (radr::generator<size_t>));
#endif
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

    using cit_t  = radr::detail::filter_iterator<radr::const_iterator_t<container_t>,
                                                radr::const_iterator_t<container_t>,
                                                std::remove_cvref_t<decltype(fn)>>;
    using csen_t = std::default_sentinel_t;

    static constexpr radr::borrowing_rad_kind bk = radr::borrowing_rad_kind::unsized;
    using borrow_t                               = radr::borrowing_rad<cit_t, csen_t, cit_t, csen_t, bk>;

    template <typename in_t>
    static void type_checks_impl()
    {
        /* guaranteed for all filter adaptors */
        EXPECT_TRUE(radr::constant_range<in_t>);
        EXPECT_TRUE(radr::const_symmetric_range<in_t>);

        /* preserved for all filter adaptors */
        EXPECT_EQ(std::ranges::bidirectional_range<in_t>, std::ranges::bidirectional_range<container_t>);

        /* never preserved for filter adaptors */
        EXPECT_FALSE(std::ranges::common_range<in_t>);
        EXPECT_FALSE(std::ranges::sized_range<in_t>);
        EXPECT_FALSE(std::ranges::random_access_range<in_t>);
        EXPECT_FALSE(std::ranges::contiguous_range<in_t>);
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

using container_types = ::testing::Types<std::forward_list<size_t>, // unsized
                                         std::list<size_t>,         // sized without sized_sentinel
                                         std::deque<size_t>,
                                         std::vector<size_t>>;

TYPED_TEST_SUITE(filter_forward, container_types);

TYPED_TEST(filter_forward, rvalue)
{
    using container_t = TestFixture::container_t;
    using borrow_t    = TestFixture::borrow_t;

    auto ra = std::move(this->in) | radr::filter(fn);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(filter_forward, lvalue)
{
    using borrow_t = TestFixture::borrow_t;

    auto ra = std::ref(this->in) | radr::filter(fn);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), borrow_t);

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(filter_forward, chained_adaptor)
{
    auto tru = [](auto)
    {
        return true;
    };
    using chained_pred_t = radr::detail::and_fn<std::remove_cvref_t<decltype(fn)>, decltype(tru)>;

    using container_t = TestFixture::container_t;
    using cit_t       = radr::detail::
      filter_iterator<radr::const_iterator_t<container_t>, radr::const_iterator_t<container_t>, chained_pred_t>;
    using csen_t = std::default_sentinel_t;

    static constexpr radr::borrowing_rad_kind bk = radr::borrowing_rad_kind::unsized;
    using borrow_t                               = radr::borrowing_rad<cit_t, csen_t, cit_t, csen_t, bk>;

    auto ra = std::ref(this->in) | radr::filter(fn) | radr::filter(tru);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), borrow_t);
    TestFixture::template type_checks<decltype(ra)>();
}

// --------------------------------------------------------------------------
// forward tests for non common ranges
// --------------------------------------------------------------------------

TEST(filter_forward_, noncommon)
{
    /* construct container */
    auto in           = std::list<size_t>{1, 2, 3, 4, 5, 6, 7} | radr::take(6);
    using container_t = decltype(in);
    static_assert(!std::ranges::common_range<container_t>);

    /* expected type */
    using pred_t = std::remove_cvref_t<decltype(fn)>;
    using cit_t =
      radr::detail::filter_iterator<radr::const_iterator_t<container_t>, radr::const_sentinel_t<container_t>, pred_t>;
    using csen_t = std::default_sentinel_t;

    static constexpr radr::borrowing_rad_kind bk = radr::borrowing_rad_kind::unsized;
    using borrow_t                               = radr::borrowing_rad<cit_t, csen_t, cit_t, csen_t, bk>;

    auto ra = std::ref(in) | radr::filter(fn);
    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), borrow_t);
}

TEST(filter_forward_, noncommon_chained)
{
    /* construct container */
    auto in           = std::forward_list<size_t>{1, 2, 3, 4, 5, 6, 7} | radr::take(6);
    using container_t = decltype(in);
    static_assert(!std::ranges::common_range<container_t>);

    /* alibi predicate */
    auto tru = [](auto)
    {
        return true;
    };

    /* expected type */
    using pred_t = radr::detail::and_fn<std::remove_cvref_t<decltype(fn)>, decltype(tru)>;
    using cit_t =
      radr::detail::filter_iterator<radr::const_iterator_t<container_t>, radr::const_sentinel_t<container_t>, pred_t>;
    using csen_t = std::default_sentinel_t;

    static constexpr radr::borrowing_rad_kind bk = radr::borrowing_rad_kind::unsized;
    using borrow_t                               = radr::borrowing_rad<cit_t, csen_t, cit_t, csen_t, bk>;

    auto ra = std::ref(in) | radr::filter(fn) | radr::filter(tru);
    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), borrow_t);
}

TEST(filter_forward_, noncommon2common)
{
    /* construct container */
    auto in           = std::list<size_t>{1, 1, 2, 2, 1, 1} | radr::take(6);
    using container_t = decltype(in);
    static_assert(!std::ranges::common_range<container_t>);

    {
        auto ra = std::ref(in) | radr::filter(fn);
        auto e  = radr::begin(ra);
        std::ranges::advance(e, radr::end(ra));

        auto eager_e = std::ranges::next(in.begin(), 6); // past the underlying end
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE <= 12)
        (void)eager_e; // bug libstdc++
#else
        EXPECT_EQ(e.base_iter(), eager_e);
#endif
    }

    {
        auto ra = std::ref(in) | radr::filter(fn) | radr::to_common;
        auto e  = radr::begin(ra);
        std::ranges::advance(e, radr::end(ra));

        auto eager_e = std::ranges::next(in.begin(), 4); // first '1' in tail
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE <= 12)
        (void)eager_e; // bug libstdc++
#else
        EXPECT_EQ(e.base_iter(), eager_e);
#endif
    }
}

// --------------------------------------------------------------------------
// owning copy test
// --------------------------------------------------------------------------

TEST(filter, owning_copy_test)
{
    auto own = std::vector{1, 2, 3, 4, 5, 6} | radr::filter(fn);
    EXPECT_RANGE_EQ(own, comp);

    auto cpy = own;
    EXPECT_RANGE_EQ(own, cpy);
}
