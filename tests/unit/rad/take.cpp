#include <deque>
#include <forward_list>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/detail/detail.hpp>
#include <radr/rad/take.hpp>

#include "radr/detail/fwd.hpp"

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

inline std::vector<size_t> const comp{1, 2, 3};

// --------------------------------------------------------------------------
// input test
// --------------------------------------------------------------------------

TEST(take, input)
{
    auto ra = radr::test::iota_input_range(1, 7) | radr::take(3);

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_SAME_TYPE(decltype(ra), radr::generator<size_t>);
}

// --------------------------------------------------------------------------
// forward_range shared
// --------------------------------------------------------------------------

template <typename ra_t, typename container_t>
void type_checks_impl()
{
    /* preserved for all take adaptors */
    EXPECT_EQ(std::ranges::bidirectional_range<ra_t>, std::ranges::bidirectional_range<container_t>);
    EXPECT_EQ(std::ranges::random_access_range<ra_t>, std::ranges::random_access_range<container_t>);
    EXPECT_EQ(std::ranges::contiguous_range<ra_t>, std::ranges::contiguous_range<container_t>);
    EXPECT_EQ(std::ranges::sized_range<ra_t>, std::ranges::sized_range<container_t>);

    /* preserved only for RA+sized */
    EXPECT_EQ(std::ranges::common_range<ra_t>,
              std::ranges::random_access_range<container_t> && std::ranges::sized_range<container_t>);
}

template <typename ra_t, typename container_t>
void type_checks()
{
    radr::test::generic_adaptor_checks<ra_t, container_t>();

    type_checks_impl<ra_t, container_t>();
    type_checks_impl<ra_t const, container_t>();

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<ra_t>, size_t &);
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<ra_t const>, size_t const &);
}

// --------------------------------------------------------------------------
// forward_list test (in particular, this tests the unsized codepath)
// --------------------------------------------------------------------------

TEST(take, forward_list)
{
    using container_t = std::forward_list<size_t>;
    container_t container{1, 2, 3, 4, 5, 6};

    using it_t  = std::counted_iterator<radr::iterator_t<container_t>>;
    using sen_t = radr::detail::take_sentinel<radr::iterator_t<container_t>, radr::sentinel_t<container_t>>;
    using cit_t = std::counted_iterator<radr::const_iterator_t<container_t>>;
    using csen_t =
      radr::detail::take_sentinel<radr::const_iterator_t<container_t>, radr::const_sentinel_t<container_t>>;

    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, radr::borrowing_rad_kind::unsized>;

    /* lvalue */
    {
        auto ra = std::ref(container) | radr::take(3);

        EXPECT_RANGE_EQ(ra, comp);
        EXPECT_SAME_TYPE(decltype(ra), borrow_t);
    }

    /* rvalue */
    {
        auto ra = std::move(container) | radr::take(3);

        EXPECT_RANGE_EQ(ra, comp);
        EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));
    }

    type_checks<borrow_t, container_t>();
}

// --------------------------------------------------------------------------
// list test (in particular, this tests the sized-but-not-RA codepath)
// --------------------------------------------------------------------------

TEST(take, list)
{
    using container_t = std::list<size_t>;
    container_t container{1, 2, 3, 4, 5, 6};

    using it_t   = std::counted_iterator<radr::iterator_t<container_t>>;
    using sen_t  = std::default_sentinel_t;
    using cit_t  = std::counted_iterator<radr::const_iterator_t<container_t>>;
    using csen_t = std::default_sentinel_t;

    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, radr::borrowing_rad_kind::sized>;

    /* lvalue */
    {
        auto ra = std::ref(container) | radr::take(3);

        EXPECT_RANGE_EQ(ra, comp);
        EXPECT_SAME_TYPE(decltype(ra), borrow_t);
    }

    /* rvalue */
    {
        auto ra = std::move(container) | radr::take(3);

        EXPECT_RANGE_EQ(ra, comp);
        EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));
    }

    type_checks<borrow_t, container_t>();
}

// --------------------------------------------------------------------------
// RandomAccess+Sized test (delegates to subborrow)
// --------------------------------------------------------------------------

template <typename container_t>
void ra_sized_test()
{
    container_t container{1, 2, 3, 4, 5, 6};

    using it_t   = radr::iterator_t<container_t>;
    using sen_t  = radr::iterator_t<container_t>;
    using cit_t  = radr::const_iterator_t<container_t>;
    using csen_t = radr::const_iterator_t<container_t>;

    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, radr::borrowing_rad_kind::sized>;

    /* lvalue */
    {
        auto ra = std::ref(container) | radr::take(3);

        EXPECT_RANGE_EQ(ra, comp);
        EXPECT_SAME_TYPE(decltype(ra), borrow_t);
    }

    /* rvalue */
    {
        auto ra = std::move(container) | radr::take(3);

        EXPECT_RANGE_EQ(ra, comp);
        EXPECT_SAME_TYPE(decltype(ra), (radr::owning_rad<container_t, borrow_t>));
    }

    type_checks<borrow_t, container_t>();
}

TEST(take, deque)
{
    ra_sized_test<std::deque<size_t>>();
}

TEST(take, vector)
{
    ra_sized_test<std::vector<size_t>>();
}
