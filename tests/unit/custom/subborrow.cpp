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
#include <radr/custom/subborrow.hpp>
#include <radr/rad_util/borrowing_rad.hpp>

// --------------------------------------------------------------------------
// vector
// --------------------------------------------------------------------------

struct subborrow_vector : public testing::Test
{
    std::vector<int> in{1, 2, 3, 4, 5, 6, 7};

    using borrow_t = radr::borrowing_rad<int *, int *, int const *, int const *, radr::borrowing_rad_kind::sized>;
};

TEST_F(subborrow_vector, borrow)
{
    auto sub = radr::borrow(in);

    EXPECT_RANGE_EQ(in, sub);
    EXPECT_SAME_TYPE(decltype(sub), borrow_t);
}

TEST_F(subborrow_vector, subborrow_it)
{
    auto sub = radr::subborrow(in, std::next(in.begin()), std::prev(in.end()));

    EXPECT_RANGE_EQ(sub, (std::vector{2, 3, 4, 5, 6}));
    EXPECT_SAME_TYPE(decltype(sub), borrow_t);
}

TEST_F(subborrow_vector, subborrow_it_size)
{
    auto sub = radr::subborrow(in, std::next(in.begin()), std::prev(in.end()), 5);

    EXPECT_RANGE_EQ(sub, (std::vector{2, 3, 4, 5, 6}));
    EXPECT_SAME_TYPE(decltype(sub), borrow_t);
}

TEST_F(subborrow_vector, subborrow_ptr)
{
    auto sub = radr::subborrow(in, in.data() + 1, in.data() + 6);

    EXPECT_RANGE_EQ(sub, (std::vector{2, 3, 4, 5, 6}));
    EXPECT_SAME_TYPE(decltype(sub), borrow_t);
}

TEST_F(subborrow_vector, subborrow_index)
{
    auto sub = radr::subborrow(in, 1, 6);

    EXPECT_RANGE_EQ(sub, (std::vector{2, 3, 4, 5, 6}));
    EXPECT_SAME_TYPE(decltype(sub), borrow_t);
}

// --------------------------------------------------------------------------
// list
// --------------------------------------------------------------------------

struct subborrow_list : public testing::Test
{
    std::list<int> in{1, 2, 3, 4, 5, 6, 7};

    using it  = std::list<int>::iterator;
    using cit = std::list<int>::const_iterator;

    using sized_borrow_t   = radr::borrowing_rad<it, it, cit, cit, radr::borrowing_rad_kind::sized>;
    using unsized_borrow_t = radr::borrowing_rad<it, it, cit, cit, radr::borrowing_rad_kind::unsized>;
};

TEST_F(subborrow_list, borrow)
{
    auto sub = radr::borrow(in);

    EXPECT_RANGE_EQ(in, sub);
    EXPECT_SAME_TYPE(decltype(sub), sized_borrow_t);
}

TEST_F(subborrow_list, subborrow_it)
{
    auto sub = radr::subborrow(in, std::next(in.begin()), std::prev(in.end()));

    EXPECT_RANGE_EQ(sub, (std::list{2, 3, 4, 5, 6}));
    EXPECT_SAME_TYPE(decltype(sub), unsized_borrow_t);
    EXPECT_FALSE(std::ranges::sized_range<decltype(sub)>);
}

TEST_F(subborrow_list, subborrow_it_size)
{
    auto sub = radr::subborrow(in, std::next(in.begin()), std::prev(in.end()), 5);

    EXPECT_RANGE_EQ(sub, (std::list{2, 3, 4, 5, 6}));
    EXPECT_SAME_TYPE(decltype(sub), sized_borrow_t);
    EXPECT_TRUE(std::ranges::sized_range<decltype(sub)>);
}

// --------------------------------------------------------------------------
// borrow_single()
// --------------------------------------------------------------------------

TEST(borrow_single, type)
{
    int  i = 3;
    auto b = radr::borrow_single(i);
    EXPECT_SAME_TYPE(decltype(b), radr::borrowing_rad<int *>);
}

TEST(borrow_single, val)
{
    int  i = 3;
    auto b = radr::borrow_single(i);
    EXPECT_EQ(*b.begin(), 3);
    EXPECT_EQ(*b.data(), 3);
    EXPECT_EQ(b.size(), 1ull);
    EXPECT_EQ(b.empty(), false);
}
