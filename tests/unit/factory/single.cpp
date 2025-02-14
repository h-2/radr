#include <cstddef>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/custom/subborrow.hpp>
#include <radr/factory/single.hpp>

//---------------------------------------------------------------------
// deduction guides
//---------------------------------------------------------------------

TEST(single, deduction_guides)
{
    int              i    = 3;
    auto             iref = std::ref(i);
    std::vector<int> vec;

    EXPECT_SAME_TYPE(decltype(radr::single(iref)),
                     (radr::repeat_rng<int, radr::constant_t<1>, radr::repeat_rng_storage::indirect>));
    EXPECT_SAME_TYPE(decltype(radr::single(std::ref(i))),
                     (radr::repeat_rng<int, radr::constant_t<1>, radr::repeat_rng_storage::indirect>));

    EXPECT_SAME_TYPE(decltype(radr::single(vec)),
                     (radr::repeat_rng<std::vector<int>, radr::constant_t<1>, radr::repeat_rng_storage::in_range>));
    EXPECT_SAME_TYPE(decltype(radr::single(std::vector{3})),
                     (radr::repeat_rng<std::vector<int>, radr::constant_t<1>, radr::repeat_rng_storage::in_range>));

    EXPECT_SAME_TYPE(decltype(radr::single(i)),
                     (radr::repeat_rng<int const, radr::constant_t<1>, radr::repeat_rng_storage::in_iterator>));
    EXPECT_SAME_TYPE(decltype(radr::single(3)),
                     (radr::repeat_rng<int const, radr::constant_t<1>, radr::repeat_rng_storage::in_iterator>));
}

//---------------------------------------------------------------------
// indirect
//---------------------------------------------------------------------

using in_ind_t        = radr::repeat_rng<int, radr::constant_t<1>, radr::repeat_rng_storage::indirect>;
using in_cind_t       = radr::repeat_rng<int const, radr::constant_t<1>, radr::repeat_rng_storage::indirect>;
using in_ind_borrow_t = radr::repeat_rng<int const, ptrdiff_t, radr::repeat_rng_storage::indirect>;

TEST(single_indirect, concepts)
{
    EXPECT_TRUE(std::ranges::contiguous_range<in_ind_t>);
    EXPECT_TRUE(std::ranges::borrowed_range<in_ind_t>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<in_ind_t>, int *);
    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<in_ind_t, std::vector<int> /*irrelevant*/>();

    EXPECT_EQ(sizeof(in_ind_t), 16);
}

TEST(single_indirect, concepts_const)
{
    EXPECT_TRUE(std::ranges::contiguous_range<in_cind_t>);
    EXPECT_TRUE(std::ranges::borrowed_range<in_cind_t>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<in_cind_t>, int const *);
    radr::test::generic_adaptor_checks<in_cind_t, std::vector<int> /*irrelevant*/>();
    EXPECT_EQ(sizeof(in_ind_t), 16);
}

TEST(single_indirect, members)
{
    int i = 3;

    in_ind_t r{i};
    EXPECT_EQ(*r.begin(), 3);
    EXPECT_EQ(*r.data(), 3);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_indirect, members_const)
{
    int i = 3;

    in_cind_t c{i};
    EXPECT_EQ(*c.begin(), 3);
    EXPECT_EQ(*c.data(), 3);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

TEST(single_indirect, default_)
{
    in_ind_t r{};

    EXPECT_EQ(r.begin(), nullptr);
    // Dereferencing is UB
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_indirect, default_const)
{
    in_cind_t c{};

    EXPECT_EQ(c.begin(), nullptr);
    // Dereferencing is UB
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

TEST(single_indirect, subborrow_same_size)
{
    int       i = 3;
    in_cind_t r{i};

    auto sub = radr::subborrow(r, r.begin(), r.begin() + 1);

    EXPECT_SAME_TYPE(decltype(sub), in_ind_borrow_t);
    EXPECT_EQ(sub.size(), 1);
    EXPECT_EQ(sub[0], 3);
}

TEST(single_indirect, subborrow_zero_size)
{
    int       i = 3;
    in_cind_t r{i};

    auto sub = radr::subborrow(r, r.begin(), r.begin());

    EXPECT_SAME_TYPE(decltype(sub), in_ind_borrow_t);
    EXPECT_EQ(sub.size(), 0);
}

TEST(single_indirect, subborrow_over_size)
{
    int       i = 3;
    in_cind_t r{i};

    auto sub = radr::subborrow(r, r.begin(), r.begin() + 40, 40);

    EXPECT_SAME_TYPE(decltype(sub), in_ind_borrow_t);
    EXPECT_EQ(sub.size(), 1);
    EXPECT_EQ(sub[0], 3);
}
//---------------------------------------------------------------------
// in_range
//---------------------------------------------------------------------

using in_r_t  = radr::repeat_rng<int, radr::constant_t<1>, radr::repeat_rng_storage::in_range>;
using in_cr_t = radr::repeat_rng<int const, radr::constant_t<1>, radr::repeat_rng_storage::in_range>;
template <typename Rng>
using in_r_borrow_t = radr::borrowing_rad<radr::iterator_t<Rng>,
                                          radr::iterator_t<Rng>,
                                          radr::const_iterator_t<Rng>,
                                          radr::const_iterator_t<Rng>,
                                          radr::borrowing_rad_kind::sized>;

TEST(single_in_range, concepts)
{
    EXPECT_TRUE(std::ranges::contiguous_range<in_r_t>);
    EXPECT_FALSE(std::ranges::borrowed_range<in_r_t>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<in_r_t>, int *);
    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<in_r_t, std::vector<int> /*irrelevant*/>();
    EXPECT_EQ(sizeof(in_ind_t), 16);
}

TEST(single_in_range, concepts_const)
{
    EXPECT_TRUE(std::ranges::contiguous_range<in_cr_t>);
    EXPECT_FALSE(std::ranges::borrowed_range<in_cr_t>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<in_cr_t>, int const *);
    radr::test::generic_adaptor_checks<in_cr_t, std::vector<int> /*irrelevant*/>();
    EXPECT_EQ(sizeof(in_ind_t), 16);
}

TEST(single_in_range, members)
{
    in_r_t r{3};
    EXPECT_EQ(*r.begin(), 3);
    EXPECT_EQ(*r.data(), 3);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_in_range, members_const)
{
    radr::repeat_rng<int const, radr::constant_t<1>, radr::repeat_rng_storage::in_range> c{3};
    EXPECT_EQ(*c.begin(), 3);
    EXPECT_EQ(*c.data(), 3);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

TEST(single_in_range, default_)
{
    in_r_t r{};

    EXPECT_EQ(*r.begin(), 0);
    EXPECT_EQ(*r.data(), 0);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_in_range, default_const)
{
    radr::repeat_rng<int const, radr::constant_t<1>, radr::repeat_rng_storage::in_range> c{};

    EXPECT_EQ(*c.begin(), 0);
    EXPECT_EQ(*c.data(), 0);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

TEST(single_in_range, subborrow_same_size)
{
    in_r_t r{3};

    auto sub = radr::subborrow(r, r.begin(), r.begin() + 1);

    EXPECT_SAME_TYPE(decltype(sub), in_r_borrow_t<in_r_t>);
    EXPECT_EQ(sub.size(), 1);
    EXPECT_EQ(sub[0], 3);
}

TEST(single_in_range, subborrow_zero_size)
{
    in_r_t r{3};

    auto sub = radr::subborrow(r, r.begin(), r.begin());

    EXPECT_SAME_TYPE(decltype(sub), in_r_borrow_t<in_r_t>);
    EXPECT_EQ(sub.size(), 0);
}

// TEST(single_in_range, subborrow_over_size) // doesn't work && assert prevents this

//---------------------------------------------------------------------
// in_iterator
//---------------------------------------------------------------------

using in_it_t        = radr::repeat_rng<int, radr::constant_t<1>, radr::repeat_rng_storage::in_iterator>;
using in_cit_t       = radr::repeat_rng<int const, radr::constant_t<1>, radr::repeat_rng_storage::in_iterator>;
using in_it_borrow_t = radr::repeat_rng<int const, ptrdiff_t, radr::repeat_rng_storage::in_iterator>;

TEST(single_in_iterator, concepts)
{
    EXPECT_FALSE(std::ranges::contiguous_range<in_it_t>);
    EXPECT_TRUE(std::ranges::random_access_range<in_it_t>);
    EXPECT_TRUE(std::ranges::borrowed_range<in_it_t>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<in_it_t>,
                     (radr::detail::repeat_iterator<int const, radr::repeat_rng_storage::in_iterator>));
    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<in_it_t, std::vector<int> /*irrelevant*/>();
    EXPECT_EQ(sizeof(in_ind_t), 16);
}

TEST(single_in_iterator, concepts_const)
{
    EXPECT_FALSE(std::ranges::contiguous_range<in_cit_t>);
    EXPECT_TRUE(std::ranges::random_access_range<in_cit_t>);
    EXPECT_TRUE(std::ranges::borrowed_range<in_cit_t>);
    EXPECT_SAME_TYPE(std::ranges::iterator_t<in_cit_t>,
                     (radr::detail::repeat_iterator<int const, radr::repeat_rng_storage::in_iterator>));
    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<in_cit_t, std::vector<int> /*irrelevant*/>();
    EXPECT_EQ(sizeof(in_ind_t), 16);
}

TEST(single_in_iterator, members)
{
    in_it_t r{3};
    EXPECT_EQ(*r.begin(), 3);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_in_iterator, members_const)
{
    in_cit_t c{3};
    EXPECT_EQ(*c.begin(), 3);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

TEST(single_in_iterator, default_)
{
    in_it_t r{};

    EXPECT_EQ(*r.begin(), 0);
    EXPECT_EQ(r.size(), 1ull);
    EXPECT_EQ(r.empty(), false);
}

TEST(single_in_iterator, default_const)
{
    in_cit_t c{};

    EXPECT_EQ(*c.begin(), 0);
    EXPECT_EQ(c.size(), 1ull);
    EXPECT_EQ(c.empty(), false);
}

TEST(single_in_iterator, subborrow_same_size)
{
    in_cit_t r{3};

    auto sub = radr::subborrow(r, r.begin(), r.begin() + 1);

    EXPECT_SAME_TYPE(decltype(sub), in_it_borrow_t);
    EXPECT_EQ(sub.size(), 1);
    EXPECT_EQ(sub[0], 3);
}

TEST(single_in_iterator, subborrow_zero_size)
{
    in_cit_t r{3};

    auto sub = radr::subborrow(r, r.begin(), r.begin());

    EXPECT_SAME_TYPE(decltype(sub), in_it_borrow_t);
    EXPECT_EQ(sub.size(), 0);
}

TEST(single_in_iterator, subborrow_over_size)
{
    in_cit_t r{3};

    auto sub = radr::subborrow(r, r.begin(), r.begin() + 40, 40);

    EXPECT_SAME_TYPE(decltype(sub), in_it_borrow_t);
    EXPECT_EQ(sub.size(), 1);
    EXPECT_EQ(sub[0], 3);
}
