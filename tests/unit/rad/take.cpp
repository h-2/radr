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
#ifdef __cpp_lib_generator
    EXPECT_SAME_TYPE(decltype(ra), (radr::generator<size_t &&, size_t>));
#else
    EXPECT_SAME_TYPE(decltype(ra), (radr::generator<size_t>));
#endif
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

template <typename container_t, typename borrow_t>
void forward_range_test()
{
    container_t container{1, 2, 3, 4, 5, 6};

    /* lvalue */
    {
        auto ra = std::ref(container) | radr::take(3);

        EXPECT_RANGE_EQ(ra, comp);
        EXPECT_SAME_TYPE(decltype(ra), borrow_t);
    }

    /* borrowed */
    {
        auto b  = radr::borrow(container);
        auto ra = b | radr::take(3);

        EXPECT_RANGE_EQ(ra, comp);
        EXPECT_SAME_TYPE(decltype(ra), borrow_t);
    }

    /* lvalue, folding */
    {
        auto ra = std::ref(container) | radr::take(4) | radr::take(3);

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
// forward_list test (in particular, this tests the unsized codepath)
// --------------------------------------------------------------------------

TEST(take, forward_list)
{
    using container_t = std::forward_list<size_t>;

    using it_t  = std::counted_iterator<radr::iterator_t<container_t>>;
    using sen_t = radr::detail::take_sentinel<radr::iterator_t<container_t>, radr::sentinel_t<container_t>>;
    using cit_t = std::counted_iterator<radr::const_iterator_t<container_t>>;
    using csen_t =
      radr::detail::take_sentinel<radr::const_iterator_t<container_t>, radr::const_sentinel_t<container_t>>;

    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, radr::borrowing_rad_kind::unsized>;

    forward_range_test<container_t, borrow_t>();
}

TEST(take, owning_copy_test)
{
    auto own = std::forward_list{1, 2, 3, 4, 5, 6} | radr::take(3);
    EXPECT_RANGE_EQ(own, comp);

    auto cpy = own;
    EXPECT_RANGE_EQ(own, cpy);
}

// --------------------------------------------------------------------------
// list test (in particular, this tests the sized-but-not-RA codepath)
// --------------------------------------------------------------------------

TEST(take, list)
{
    using container_t = std::list<size_t>;

    using it_t   = std::counted_iterator<radr::iterator_t<container_t>>;
    using sen_t  = std::default_sentinel_t;
    using cit_t  = std::counted_iterator<radr::const_iterator_t<container_t>>;
    using csen_t = std::default_sentinel_t;

    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, radr::borrowing_rad_kind::sized>;

    forward_range_test<container_t, borrow_t>();
}

// --------------------------------------------------------------------------
// RandomAccess+Sized test (delegates to subborrow)
// --------------------------------------------------------------------------

TEST(take, deque)
{
    using container_t = std::deque<size_t>;

    using it_t   = radr::iterator_t<container_t>;
    using sen_t  = radr::iterator_t<container_t>;
    using cit_t  = radr::const_iterator_t<container_t>;
    using csen_t = radr::const_iterator_t<container_t>;

    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, radr::borrowing_rad_kind::sized>;

    forward_range_test<container_t, borrow_t>();
}

TEST(take, vector)
{
    using container_t = std::vector<size_t>;

    using it_t   = radr::iterator_t<container_t>;
    using sen_t  = radr::iterator_t<container_t>;
    using cit_t  = radr::const_iterator_t<container_t>;
    using csen_t = radr::const_iterator_t<container_t>;

    using borrow_t = radr::borrowing_rad<it_t, sen_t, cit_t, csen_t, radr::borrowing_rad_kind::sized>;

    forward_range_test<container_t, borrow_t>();
}

// --------------------------------------------------------------------------
// pipe tests (these are independent of take and only test detail/pipe.hpp
// --------------------------------------------------------------------------

TEST(pipe, function_call_test)
{
    constexpr bool owning_rval = requires { radr::take(std::vector{1, 2, 3, 4, 5, 6}, 1); };
    // constexpr bool owning_lval = requires (std::vector<int> vec) { radr::take(vec, 1); };

    constexpr bool borrowed_rval = requires { radr::take(std::string_view{"foo"}, 1); };
    constexpr bool borrowed_lval = requires(std::string_view v) { radr::take(v, 1); };

    constexpr bool singlepass_rval = requires { radr::take(radr::test::iota_input_range(1, 1), 1); };
    // constexpr bool singlepass_lval = requires (radr::generator<int> g) { radr::take(g, 1); };

    EXPECT_TRUE(owning_rval);
    // EXPECT_FALSE(owning_lval);

    EXPECT_TRUE(borrowed_rval);
    EXPECT_TRUE(borrowed_lval);

    EXPECT_TRUE(singlepass_rval);
    // EXPECT_FALSE(singlepass_lval);
}

TEST(pipe, pipe_test)
{
    constexpr bool owning_rval = requires { std::vector{1, 2, 3, 4, 5, 6} | radr::take(1); };
    // constexpr bool owning_lval = requires (std::vector<int> vec) { vec | radr::take(1); };

    constexpr bool borrowed_rval = requires { std::string_view{"foo"} | radr::take(1); };
    constexpr bool borrowed_lval = requires(std::string_view v) { v | radr::take(1); };

    constexpr bool singlepass_rval = requires { radr::test::iota_input_range(1, 1) | radr::take(1); };
    // constexpr bool singlepass_lval = requires (radr::generator<int> g) { g | radr::take(1); };

    EXPECT_TRUE(owning_rval);
    // EXPECT_FALSE(owning_lval);

    EXPECT_TRUE(borrowed_rval);
    EXPECT_TRUE(borrowed_lval);

    EXPECT_TRUE(singlepass_rval);
    // EXPECT_FALSE(singlepass_lval);
}
