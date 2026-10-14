#include <deque>
#include <forward_list>
#include <list>
#include <memory>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/detail/detail.hpp>
#include <radr/factory/iota.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/take.hpp>
#include <radr/rad/take_while.hpp>
#include <radr/rad/transform.hpp>

using radr::detail::decay_copy;
using radr::test::range_cat;

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

inline std::vector<int>       vec{1, 2, 3, 4, 5};
inline std::deque<int>        deq{1, 2, 3, 4, 5};
inline std::list<int>         lst{1, 2, 3, 4, 5};
inline std::forward_list<int> flst{1, 2, 3, 4, 5};

// only uses *it, so that it works on all iterator categories
inline constexpr auto times2 = [](auto it)
{
    return *it * 2;
};
using times2_t = std::remove_cvref_t<decltype(times2)>;

inline std::vector<int> const comp{2, 4, 6, 8, 10};

// {1,2,3,5} (filtered out the 4) -> {2,4,6,10}
inline auto const not_four = [](int i)
{
    return i != 4;
};
inline std::vector<int> const filtered_comp{2, 4, 6, 10};

// {1,2,3} (take_while < 4) -> {2,4,6}
inline auto const less_four = [](int i)
{
    return i < 4;
};
inline std::vector<int> const taken_comp{2, 4, 6};

// --------------------------------------------------------------------------
// single-pass tests
// --------------------------------------------------------------------------

// no single-pass version; radr::detail::transform_it is multi-pass only

// --------------------------------------------------------------------------
// multi-pass tests I – canonical cases
// --------------------------------------------------------------------------

TEST(transform_it_mp, forward)
{
    auto ra = std::ref(flst) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, comp);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .common = true, .borrowed = true});
}

TEST(transform_it_mp, forward_rvalue)
{
    auto ra = decay_copy(flst) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, comp);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .common = true});
}

TEST(transform_it_mp, bidi_common)
{
    auto ra = std::ref(lst) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, comp);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .borrowed = true});
}

TEST(transform_it_mp, bidi_common_rvalue)
{
    auto ra = decay_copy(lst) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, comp);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .sized = true, .common = true});
}

TEST(transform_it_mp, ra_sized)
{
    auto ra = std::ref(deq) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, comp);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(transform_it_mp, ra_sized_rvalue)
{
    auto ra = decay_copy(deq) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, comp);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .sized = true, .common = true});
}

TEST(transform_it_mp, contig_sized)
{
    auto ra = std::ref(vec) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, comp);
    // contiguity is never preserved
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(transform_it_mp, contig_sized_rvalue)
{
    auto ra = decay_copy(vec) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, comp);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .sized = true, .common = true});
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

// no mutate test: the result of times2 is a prvalue, so the adaptor is never mutable

TEST(transform_it_mp, empty)
{
    std::vector<int> empty;
    auto             ra = std::ref(empty) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, std::vector<int>{});
}

TEST(transform_it_mp, constant)
{
    auto ra = std::cref(vec) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, comp);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(transform_it_mp, infinite)
{
    auto ra = radr::iota(0) | radr::detail::transform_it(times2);

    auto it = radr::begin(ra);
    EXPECT_EQ(*it, 0);
    ++it;
    EXPECT_EQ(*it, 2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .infinite = true, .constant = true, .borrowed = true});
}

TEST(transform_it_mp, fwd_uncommon)
{
    auto ra = std::ref(flst) | radr::filter(not_four) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, filtered_comp);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
}

TEST(transform_it_mp, bidi_uncommon)
{
    auto ra = std::ref(vec) | radr::filter(not_four) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, filtered_comp);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .constant = true, .borrowed = true});
}

TEST(transform_it_mp, ra_nonsized)
{
    auto ra = std::ref(deq) | radr::take_while(less_four) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, taken_comp);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .borrowed = true});
}

TEST(transform_it_mp, contig_nonsized)
{
    auto ra = std::ref(vec) | radr::take_while(less_four) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, taken_comp);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

TEST(transform_it_mp, receives_iterator)
{
    // the address of the element can only be obtained from the iterator, not from a copy of the element
    auto ra = std::ref(vec) | radr::detail::transform_it([](auto it) { return std::addressof(*it); });

    ASSERT_EQ(std::ranges::size(ra), vec.size());
    for (size_t i = 0; i < vec.size(); ++i)
    {
        EXPECT_EQ(ra[i], &vec[i]);
        EXPECT_EQ(*std::ranges::next(radr::begin(ra), i), &vec[i]);
    }
}

TEST(transform_it_mp, iterator_type)
{
    auto ra = std::ref(vec) | radr::detail::transform_it(times2);

    using uit_t  = radr::iterator_t<std::vector<int>>;
    using ucit_t = radr::const_iterator_t<std::vector<int>>;
    EXPECT_SAME_TYPE(radr::iterator_t<decltype(ra)>, (radr::detail::transform_iterator<uit_t, times2_t, false>));
    EXPECT_SAME_TYPE(radr::const_iterator_t<decltype(ra)>, (radr::detail::transform_iterator<ucit_t, times2_t, false>));
}

TEST(transform_it_mp, fold_transform_over_transform_it)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto ra = std::ref(vec) | radr::detail::transform_it(times2) | radr::transform(plus1);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{3, 5, 7, 9, 11}));

    using uit_t  = radr::iterator_t<std::vector<int>>;
    using nest_t = radr::detail::transform::nest_fn<times2_t, decltype(plus1)>;
    EXPECT_SAME_TYPE(radr::iterator_t<decltype(ra)>, (radr::detail::transform_iterator<uit_t, nest_t, false>));
}

TEST(transform_it_mp, no_fold_transform_it_over_transform)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto ra = std::ref(vec) | radr::transform(plus1) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{4, 6, 8, 10, 12}));

    using uit_t   = radr::iterator_t<std::vector<int>>;
    using inner_t = radr::detail::transform_iterator<uit_t, decltype(plus1)>;
    EXPECT_SAME_TYPE(radr::iterator_t<decltype(ra)>, (radr::detail::transform_iterator<inner_t, times2_t, false>));
}

// regression test: folding radr::transform on a non-common range used the wrong functor type for the sentinel
TEST(transform_it_mp, fold_transform_noncommon)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto times10 = [](int i)
    {
        return i * 10;
    };
    auto ra = std::ref(vec) | radr::take_while(less_four) | radr::transform(plus1) | radr::transform(times10);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{20, 30, 40}));

    using urange_t = decltype(std::ref(vec) | radr::take_while(less_four));
    using nest_t   = radr::detail::transform::nest_fn<decltype(plus1), decltype(times10)>;
    EXPECT_SAME_TYPE(
      radr::sentinel_t<decltype(ra)>,
      (radr::detail::transform_sentinel<radr::iterator_t<urange_t>, radr::sentinel_t<urange_t>, nest_t>));
}

TEST(transform_it_mp, fold_transform_infinite)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto ra = radr::iota(0) | radr::detail::transform_it(times2) | radr::transform(plus1);

    auto it = radr::begin(ra);
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 3);

    using uit_t  = radr::iterator_t<decltype(radr::iota(0))>;
    using nest_t = radr::detail::transform::nest_fn<times2_t, decltype(plus1)>;
    EXPECT_SAME_TYPE(radr::iterator_t<decltype(ra)>, (radr::detail::transform_iterator<uit_t, nest_t, false>));
    EXPECT_SAME_TYPE(radr::sentinel_t<decltype(ra)>, std::unreachable_sentinel_t);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .infinite = true, .constant = true, .borrowed = true});
}

TEST(transform_it_mp, no_fold_transform_it_infinite)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto ra = radr::iota(0) | radr::transform(plus1) | radr::detail::transform_it(times2);

    auto it = radr::begin(ra);
    EXPECT_EQ(*it, 2);
    ++it;
    EXPECT_EQ(*it, 4);

    EXPECT_SAME_TYPE(radr::sentinel_t<decltype(ra)>, std::unreachable_sentinel_t);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .infinite = true, .constant = true, .borrowed = true});
}

// radr::take_while preserves the iterator but holds its logic in the sentinel → must not be folded
TEST(transform_it_mp, no_fold_through_take_while)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto ra = std::ref(vec) | radr::detail::transform_it(times2) | radr::take_while([](int i) { return i < 7; }) |
              radr::transform(plus1);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{3, 5, 7}));

    using inner_t = radr::detail::transform_iterator<radr::iterator_t<std::vector<int>>, times2_t, false>;
    EXPECT_SAME_TYPE(radr::iterator_t<decltype(ra)>, (radr::detail::transform_iterator<inner_t, decltype(plus1)>));
}

TEST(transform_it_mp, no_fold_through_take_while_noncommon)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto urange =
      std::ref(vec) | radr::filter(not_four) | radr::transform(plus1) | radr::take_while([](int i) { return i < 5; });
    auto ra = urange | radr::detail::transform_it(times2) | radr::transform(plus1);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{5, 7, 9}));

    // take_while preserves the transform_iterator
    using filter_it_t = radr::iterator_t<decltype(std::ref(vec) | radr::filter(not_four))>;
    using inner_t     = radr::iterator_t<decltype(urange)>;
    EXPECT_SAME_TYPE(inner_t, (radr::detail::transform_iterator<filter_it_t, decltype(plus1)>));

    using nest_t = radr::detail::transform::nest_fn<times2_t, decltype(plus1)>;
    // the outer transform is folded into transform_it, but neither is folded into the transform below take_while
    EXPECT_SAME_TYPE(radr::iterator_t<decltype(ra)>, (radr::detail::transform_iterator<inner_t, nest_t, false>));
}

TEST(transform_it_mp, default_sentinel)
{
    auto ra = std::ref(lst) | radr::take(3) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{2, 4, 6}));
    EXPECT_SAME_TYPE(radr::sentinel_t<decltype(ra)>, std::default_sentinel_t);
    EXPECT_SAME_TYPE(radr::const_sentinel_t<decltype(ra)>, std::default_sentinel_t);
    EXPECT_TRUE((std::sized_sentinel_for<std::default_sentinel_t, radr::iterator_t<decltype(ra)>>));
    EXPECT_EQ(radr::end(ra) - radr::begin(ra), 3);
    EXPECT_EQ(radr::begin(ra) - radr::end(ra), -3);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .sized = true, .borrowed = true});
}

TEST(transform_it_mp, fold_transform_default_sentinel)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto ra = std::ref(lst) | radr::take(3) | radr::detail::transform_it(times2) | radr::transform(plus1);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{3, 5, 7}));

    using uit_t  = radr::iterator_t<decltype(std::ref(lst) | radr::take(3))>;
    using nest_t = radr::detail::transform::nest_fn<times2_t, decltype(plus1)>;
    EXPECT_SAME_TYPE(radr::iterator_t<decltype(ra)>, (radr::detail::transform_iterator<uit_t, nest_t, false>));
    EXPECT_SAME_TYPE(radr::sentinel_t<decltype(ra)>, std::default_sentinel_t);
}

TEST(transform_it_mp, no_fold_transform_it_default_sentinel)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto ra = std::ref(lst) | radr::take(3) | radr::transform(plus1) | radr::detail::transform_it(times2);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{4, 6, 8}));

    using uit_t   = radr::iterator_t<decltype(std::ref(lst) | radr::take(3))>;
    using inner_t = radr::detail::transform_iterator<uit_t, decltype(plus1)>;
    EXPECT_SAME_TYPE(radr::iterator_t<decltype(ra)>, (radr::detail::transform_iterator<inner_t, times2_t, false>));
    EXPECT_SAME_TYPE(radr::sentinel_t<decltype(ra)>, std::default_sentinel_t);
}

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(transform_it_mp, deep_copy)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::detail::transform_it(times2));
    T cpy;
    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::detail::transform_it(times2);
        EXPECT_RANGE_EQ(own, comp);
        cpy = own;
    }
    EXPECT_RANGE_EQ(cpy, comp);
}
