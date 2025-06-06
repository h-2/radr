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
#include <radr/rad/take.hpp>
#include <radr/rad/take_while.hpp>
#include <radr/rad/to_common.hpp>
#include <radr/range_access.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

constexpr auto fn = [](size_t const i)
{
    return i < 4;
};
inline std::vector<size_t> const comp{1, 2, 3};

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

TEST(take_while, input)
{
    auto ra = radr::test::iota_input_range(1, 7) | radr::take_while(fn);

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
struct take_while_forward : public testing::Test
{
    /* data members */
    _container_t in{1, 2, 3, 4, 5, 6};

    /* type foo */
    using container_t = _container_t;

    using it_t   = radr::iterator_t<container_t>;
    using sen_t  = radr::detail::take_while_sentinel<radr::iterator_t<container_t>,
                                                    radr::iterator_t<container_t>,
                                                    std::remove_cvref_t<decltype(fn)>>;
    using cit_t  = radr::const_iterator_t<container_t>;
    using csen_t = radr::detail::take_while_sentinel<radr::const_iterator_t<container_t>,
                                                     radr::const_iterator_t<container_t>,
                                                     std::remove_cvref_t<decltype(fn)>>;

    static constexpr radr::borrowing_rad_kind bk = radr::borrowing_rad_kind::unsized;
    using borrow_t                               = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, bk>;

    template <typename in_t, typename container_t_>
    static void type_checks_impl()
    {
        /* guaranteed for all take_while adaptors */
        EXPECT_TRUE(radr::mp_range<in_t>);

        /* preserved for all take_while adaptors */
        EXPECT_EQ(std::ranges::bidirectional_range<in_t>, std::ranges::bidirectional_range<container_t_>);
        EXPECT_EQ(std::ranges::random_access_range<in_t>, std::ranges::random_access_range<container_t_>);
        EXPECT_EQ(std::ranges::contiguous_range<in_t>, std::ranges::contiguous_range<container_t_>);
        EXPECT_EQ(radr::constant_range<in_t>, radr::constant_range<container_t_>);
        EXPECT_EQ(radr::const_symmetric_range<in_t>, radr::const_symmetric_range<container_t_>);

        /* never preserved for take_while adaptors */
        EXPECT_FALSE(radr::common_range<in_t>);
        EXPECT_FALSE(std::ranges::sized_range<in_t>);
    }

    template <typename in_t>
    static void type_checks()
    {
        radr::test::generic_adaptor_checks<in_t, container_t>();

        type_checks_impl<in_t, container_t>();
        type_checks_impl<in_t const, container_t const>();

        EXPECT_SAME_TYPE(std::ranges::range_reference_t<in_t>, size_t &);
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<in_t const>, size_t const &);
    }
};

using container_types = ::testing::Types<std::forward_list<size_t>, // unsized
                                         std::list<size_t>,         // sized without sized_sentinel
                                         std::deque<size_t>,
                                         std::vector<size_t>>;

TYPED_TEST_SUITE(take_while_forward, container_types);

TYPED_TEST(take_while_forward, rvalue)
{
    using container_t = TestFixture::container_t;
    using borrow_t    = TestFixture::borrow_t;

    auto ra = std::move(this->in) | radr::take_while(fn);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(take_while_forward, lvalue)
{
    using borrow_t = TestFixture::borrow_t;

    auto ra = std::ref(this->in) | radr::take_while(fn);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), borrow_t);

    TestFixture::template type_checks<decltype(ra)>();
}

TYPED_TEST(take_while_forward, to_common)
{
    auto ra = std::ref(this->in) | radr::take_while(fn);

    EXPECT_RANGE_EQ(ra, comp);
    using ra_t = decltype(ra);
    EXPECT_SAME_TYPE(radr::iterator_t<ra_t>, radr::iterator_t<typename TestFixture::container_t>);
    EXPECT_DIFFERENT_TYPE(radr::sentinel_t<ra_t>, radr::sentinel_t<typename TestFixture::container_t>);
    EXPECT_SAME_TYPE(radr::const_iterator_t<ra_t>, radr::const_iterator_t<typename TestFixture::container_t>);
    EXPECT_DIFFERENT_TYPE(radr::const_sentinel_t<ra_t>, radr::const_sentinel_t<typename TestFixture::container_t>);

    auto rac       = ra | radr::to_common;
    using common_t = decltype(rac);
    EXPECT_SAME_TYPE(radr::iterator_t<common_t>, radr::iterator_t<typename TestFixture::container_t>);
    EXPECT_SAME_TYPE(radr::sentinel_t<common_t>, radr::sentinel_t<typename TestFixture::container_t>);
    EXPECT_SAME_TYPE(radr::const_iterator_t<common_t>, radr::const_iterator_t<typename TestFixture::container_t>);
    EXPECT_SAME_TYPE(radr::const_sentinel_t<common_t>, radr::const_sentinel_t<typename TestFixture::container_t>);
}

TYPED_TEST(take_while_forward, chained_adaptor)
{
    auto tru = [](auto)
    {
        return true;
    };
    using chained_pred_t = radr::detail::and_fn<std::remove_cvref_t<decltype(fn)>, decltype(tru)>;

    using container_t = TestFixture::container_t;
    using it_t        = TestFixture::it_t;
    using sen_t =
      radr::detail::take_while_sentinel<radr::iterator_t<container_t>, radr::iterator_t<container_t>, chained_pred_t>;
    using cit_t  = TestFixture::cit_t;
    using csen_t = radr::detail::
      take_while_sentinel<radr::const_iterator_t<container_t>, radr::const_iterator_t<container_t>, chained_pred_t>;

    static constexpr radr::borrowing_rad_kind bk = radr::borrowing_rad_kind::unsized;
    using borrow_t                               = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, bk>;

    auto ra = std::ref(this->in) | radr::take_while(fn) | radr::take_while(tru);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), borrow_t);
    TestFixture::template type_checks<decltype(ra)>();
}

// --------------------------------------------------------------------------
// forward tests for non common ranges
// --------------------------------------------------------------------------

TEST(take_while_forward_, noncommon)
{
    /* construct container */
    auto in           = std::list<size_t>{1, 2, 3, 4, 5, 6, 7} | radr::take(6);
    using container_t = decltype(in);
    static_assert(!std::ranges::common_range<container_t>);

    /* expected type */
    using pred_t = std::remove_cvref_t<decltype(fn)>;
    using it_t   = radr::iterator_t<container_t>;
    using sen_t =
      radr::detail::take_while_sentinel<radr::iterator_t<container_t>, radr::sentinel_t<container_t>, pred_t>;
    using cit_t  = radr::const_iterator_t<container_t>;
    using csen_t = radr::detail::
      take_while_sentinel<radr::const_iterator_t<container_t>, radr::const_sentinel_t<container_t>, pred_t>;

    static constexpr radr::borrowing_rad_kind bk = radr::borrowing_rad_kind::unsized;
    using borrow_t                               = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, bk>;

    auto ra = std::ref(in) | radr::take_while(fn);
    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), borrow_t);
}

TEST(take_while_forward_, noncommon_chained)
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
    using it_t   = radr::iterator_t<container_t>;
    using sen_t =
      radr::detail::take_while_sentinel<radr::iterator_t<container_t>, radr::sentinel_t<container_t>, pred_t>;
    using cit_t  = radr::const_iterator_t<container_t>;
    using csen_t = radr::detail::
      take_while_sentinel<radr::const_iterator_t<container_t>, radr::const_sentinel_t<container_t>, pred_t>;

    static constexpr radr::borrowing_rad_kind bk = radr::borrowing_rad_kind::unsized;
    using borrow_t                               = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, bk>;

    auto ra = std::ref(in) | radr::take_while(fn) | radr::take_while(tru);
    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), borrow_t);
}

// --------------------------------------------------------------------------
// deep copy test
// --------------------------------------------------------------------------

TEST(take_while, deep_copy_test)
{
    auto own = std::vector{1, 2, 3, 4, 5, 6} | radr::take_while(fn);
    EXPECT_RANGE_EQ(own, comp);

    auto cpy = own;
    EXPECT_RANGE_EQ(cpy, comp);
    EXPECT_RANGE_EQ(own, comp);
}
