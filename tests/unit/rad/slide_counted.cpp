#include <deque>
#include <forward_list>
#include <list>
#include <ranges>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/counted.hpp>
#include <radr/factory/iota.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/slide.hpp>
#include <radr/rad/slide_counted.hpp>
#include <radr/rad/take_while.hpp>

using namespace std::string_view_literals;
using radr::test::range_cat;

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// slide_counted<3> of {1,2,3,4,5} -> {1,2,3} {2,3,4} {3,4,5}
inline std::vector<int>       vec{1, 2, 3, 4, 5};
inline std::deque<int>        deq{1, 2, 3, 4, 5};
inline std::list<int>         lst{1, 2, 3, 4, 5};
inline std::forward_list<int> flst{1, 2, 3, 4, 5};

inline std::vector<std::vector<int>> const slides_of_3{
  {1, 2, 3},
  {2, 3, 4},
  {3, 4, 5}
};

// {1,2,3,5} (filtered out the 4) | slide_counted<2> -> {1,2} {2,3} {3,5}
inline auto const not_four = [](int i)
{
    return i != 4;
};

inline std::vector<std::vector<int>> const filtered_slides_of_2{
  {1, 2},
  {2, 3},
  {3, 5}
};

// {1,2,3} (take_while < 4) | slide_counted<2> -> {1,2} {2,3}
inline auto const less_four = [](int i)
{
    return i < 4;
};

inline std::vector<std::vector<int>> const taken_slides_of_2{
  {1, 2},
  {2, 3}
};

/* The inner ranges are not always common, so they are collected element-wise. */
template <typename RangeOfRanges>
std::vector<std::vector<int>> flatten(RangeOfRanges && r)
{
    std::vector<std::vector<int>> out;
    for (auto && sub : r)
    {
        auto & back = out.emplace_back();
        for (auto && elem : sub)
            back.push_back(elem);
    }
    return out;
}

// --------------------------------------------------------------------------
// single-pass tests
// --------------------------------------------------------------------------

// radr::slide_counted cannot be created on single-pass ranges.

// --------------------------------------------------------------------------
// multi-pass tests I – canonical cases
// --------------------------------------------------------------------------

TEST(slide_counted_mp, forward)
{
    auto ra = std::ref(flst) | radr::slide_counted<3>;

    EXPECT_EQ(flatten(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::fwd, .sized = true, .mut = true, .borrowed = true});
}

TEST(slide_counted_mp, forward_rvalue)
{
    auto ra = std::forward_list<int>{1, 2, 3, 4, 5} | radr::slide_counted<3>;

    EXPECT_EQ(flatten(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::fwd, .sized = true, .mut = true, .borrowed = true});
}

TEST(slide_counted_mp, bidi_common)
{
    auto ra = std::ref(lst) | radr::slide_counted<3>;

    EXPECT_EQ(flatten(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::bidi, .sized = true, .mut = true, .borrowed = true});
}

TEST(slide_counted_mp, bidi_common_rvalue)
{
    auto ra = std::list<int>{1, 2, 3, 4, 5} | radr::slide_counted<3>;

    EXPECT_EQ(flatten(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::bidi, .sized = true, .mut = true, .borrowed = true});
}

TEST(slide_counted_mp, ra_sized)
{
    auto ra = std::ref(deq) | radr::slide_counted<3>;

    EXPECT_EQ(flatten(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});

    // non-contiguous, so the windows are counted ranges
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    EXPECT_SAME_TYPE(std::ranges::iterator_t<val_t>, std::counted_iterator<std::deque<int>::iterator>);
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_counted_mp, ra_sized_rvalue)
{
    auto ra = std::deque<int>{1, 2, 3, 4, 5} | radr::slide_counted<3>;

    EXPECT_EQ(flatten(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_counted_mp, contig_sized)
{
    auto ra = std::ref(vec) | radr::slide_counted<3>;

    EXPECT_EQ(flatten(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_counted_mp, contig_sized_rvalue)
{
    auto ra = std::vector<int>{1, 2, 3, 4, 5} | radr::slide_counted<3>;

    EXPECT_EQ(flatten(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

TEST(slide_counted_mp, mutate)
{
    std::vector<int> v{1, 2, 3, 4};
    auto             ra = std::ref(v) | radr::slide_counted<2>;

    // windows overlap, so modify only the first element of each window
    for (auto sub : ra)
        *sub.begin() += 10;

    EXPECT_RANGE_EQ(v, (std::vector<int>{11, 12, 13, 4}));
}

TEST(slide_counted_mp, mutate_counted)
{
    std::list<int> l{1, 2, 3, 4};
    auto           ra = std::ref(l) | radr::slide_counted<2>;

    for (auto sub : ra)
        *sub.begin() += 10;

    EXPECT_RANGE_EQ(l, (std::vector<int>{11, 12, 13, 4}));
}

TEST(slide_counted_mp, empty)
{
    std::vector<int> v{};
    auto             ra = std::ref(v) | radr::slide_counted<2>;

    EXPECT_TRUE(std::ranges::empty(ra));
    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(slide_counted_mp, empty_bidi_common)
{
    // the end is created by decrementing from the underlying end
    std::list<int> l{};
    auto           ra = std::ref(l) | radr::slide_counted<2>;

    EXPECT_TRUE(std::ranges::empty(ra));
    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(slide_counted_mp, constant)
{
    auto ra = std::cref(vec) | radr::slide_counted<3>;

    EXPECT_EQ(flatten(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(slide_counted_mp, infinite)
{
    auto ra = radr::iota(0) | radr::slide_counted<2>;

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, (std::vector<int>{0, 1}));
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 2}));

    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .infinite = true, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(slide_counted_mp, fwd_uncommon)
{
    auto ra = std::ref(flst) | radr::filter(not_four) | radr::slide_counted<2>;

    EXPECT_EQ(flatten(ra), filtered_slides_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::fwd, .sized = true, .constant = true, .borrowed = true});
}

TEST(slide_counted_mp, bidi_uncommon)
{
    auto ra = std::ref(vec) | radr::filter(not_four) | radr::slide_counted<2>;

    EXPECT_EQ(flatten(ra), filtered_slides_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::bidi, .sized = true, .constant = true, .borrowed = true});
}

TEST(slide_counted_mp, ra_nonsized)
{
    auto ra = std::ref(deq) | radr::take_while(less_four) | radr::slide_counted<2>;

    EXPECT_EQ(flatten(ra), taken_slides_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_counted_mp, contig_nonsized)
{
    auto ra = std::ref(vec) | radr::take_while(less_four) | radr::slide_counted<2>;

    EXPECT_EQ(flatten(ra), taken_slides_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

TEST(slide_counted_mp, k1)
{
    auto expected = std::vector<std::vector<int>>{{1}, {2}, {3}, {4}, {5}};
    EXPECT_EQ(flatten(std::ref(flst) | radr::slide_counted<1>), expected);
    EXPECT_EQ(flatten(std::ref(lst) | radr::slide_counted<1>), expected);
    EXPECT_EQ(flatten(std::ref(vec) | radr::slide_counted<1>), expected);
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::slide_counted<1>), 5u);
}

TEST(slide_counted_mp, k_equals_size)
{
    auto expected = std::vector<std::vector<int>>{
      {1, 2, 3, 4, 5}
    };
    EXPECT_EQ(flatten(std::ref(flst) | radr::slide_counted<5>), expected);
    EXPECT_EQ(flatten(std::ref(lst) | radr::slide_counted<5>), expected);
    EXPECT_EQ(flatten(std::ref(deq) | radr::slide_counted<5>), expected);
    EXPECT_EQ(flatten(std::ref(vec) | radr::slide_counted<5>), expected);
    EXPECT_EQ(flatten(std::ref(vec) | radr::filter([](int) { return true; }) | radr::slide_counted<5>), expected);
}

TEST(slide_counted_mp, shorter_than_k)
{
    // forward + common
    EXPECT_TRUE(std::ranges::empty(std::ref(flst) | radr::slide_counted<6>));
    EXPECT_TRUE(std::ranges::empty(std::ref(flst) | radr::slide_counted<7>));
    // bidi + common
    EXPECT_TRUE(std::ranges::empty(std::ref(lst) | radr::slide_counted<6>));
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::slide_counted<6>), 0u);
    // ra + sized
    EXPECT_TRUE(std::ranges::empty(std::ref(vec) | radr::slide_counted<6>));
    EXPECT_EQ(std::ranges::size(std::ref(vec) | radr::slide_counted<6>), 0u);
    EXPECT_TRUE(std::ranges::empty(std::ref(deq) | radr::slide_counted<6>));
    // uncommon
    EXPECT_TRUE(std::ranges::empty(std::ref(flst) | radr::filter(not_four) | radr::slide_counted<5>));
    EXPECT_TRUE(std::ranges::empty(std::ref(vec) | radr::filter(not_four) | radr::slide_counted<5>));
    EXPECT_TRUE(std::ranges::empty(std::ref(vec) | radr::take_while(less_four) | radr::slide_counted<4>));
}

TEST(slide_counted_mp, size)
{
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::slide_counted<1>), 5u);
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::slide_counted<3>), 3u);
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::slide_counted<5>), 1u);
    EXPECT_EQ(std::ranges::size(std::ref(deq) | radr::slide_counted<4>), 2u);
    EXPECT_EQ(std::ranges::size(std::ref(vec) | radr::slide_counted<2>), 4u);
}

TEST(slide_counted_mp, same_windows_as_slide)
{
    EXPECT_EQ(flatten(std::ref(lst) | radr::slide_counted<2>), flatten(std::ref(lst) | radr::slide(2)));
    EXPECT_EQ(flatten(std::ref(vec) | radr::slide_counted<1>), flatten(std::ref(vec) | radr::slide(1)));
    EXPECT_EQ(flatten(std::ref(vec) | radr::slide_counted<5>), flatten(std::ref(vec) | radr::slide(5)));
    EXPECT_EQ(flatten(std::ref(deq) | radr::slide_counted<4>), flatten(std::ref(deq) | radr::slide(4)));
    EXPECT_EQ(flatten(std::ref(flst) | radr::filter(not_four) | radr::slide_counted<3>),
              flatten(std::ref(flst) | radr::filter(not_four) | radr::slide(3)));
}

TEST(slide_counted_mp, string_view_windows)
{
    // subborrow is used for contiguous ranges
    auto ra = "abcde"sv | radr::slide_counted<3>;

    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(ra)>, std::string_view);
    EXPECT_RANGE_EQ(ra, (std::vector{"abc"sv, "bcd"sv, "cde"sv}));
}

TEST(slide_counted_mp, reverse_iteration)
{
    auto ra = std::ref(lst) | radr::slide_counted<3>;

    auto it = ra.end();
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3, 4, 5}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2, 3, 4}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(it, ra.begin());
}

TEST(slide_counted_mp, reverse_iteration_uncommon)
{
    // no decrement from the end possible, but from any valid position
    auto ra = std::ref(vec) | radr::filter(not_four) | radr::slide_counted<2>;

    auto it = std::ranges::next(ra.begin(), 2);
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3, 5}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2, 3}));
    it--;
    EXPECT_EQ(it, ra.begin());
}

TEST(slide_counted_mp, random_access_operations)
{
    auto ra = std::ref(vec) | radr::slide_counted<3>;

    auto b = ra.begin();
    auto e = ra.end();
    EXPECT_EQ(e - b, 3);
    EXPECT_EQ(b - e, -3);
    EXPECT_RANGE_EQ(b[2], (std::vector<int>{3, 4, 5}));
    EXPECT_RANGE_EQ(*(b + 1), (std::vector<int>{2, 3, 4}));
    EXPECT_RANGE_EQ(*(1 + b), (std::vector<int>{2, 3, 4}));
    EXPECT_RANGE_EQ(*(e - 3), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(b + 3, e);
    EXPECT_LT(b, e);
    EXPECT_GT(e, b + 2);

    auto it = b;
    it += 2;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3, 4, 5}));
    it -= 2;
    EXPECT_EQ(it, b);
}

TEST(slide_counted_mp, random_access_operations_counted)
{
    auto ra = std::ref(deq) | radr::slide_counted<3>;

    auto b = ra.begin();
    auto e = ra.end();
    EXPECT_EQ(e - b, 3);
    EXPECT_RANGE_EQ(b[2], (std::vector<int>{3, 4, 5}));
    EXPECT_RANGE_EQ(*(e - 3), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(b + 3, e);
    EXPECT_LT(b, e);
}

TEST(slide_counted_mp, random_access_operations_uncommon)
{
    auto ra = std::ref(vec) | radr::take_while(less_four) | radr::slide_counted<2>;

    auto b  = ra.begin();
    auto it = b + 1;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2, 3}));
    EXPECT_RANGE_EQ(it[-1], (std::vector<int>{1, 2}));
    EXPECT_EQ(it - b, 1);
    EXPECT_LT(b, it);
    EXPECT_EQ(std::ranges::next(it), ra.end());
}

TEST(slide_counted_mp, random_access_operations_infinite)
{
    auto ra = radr::iota(0) | radr::slide_counted<3>;

    auto it = ra.begin() + 10;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{10, 11, 12}));
    EXPECT_RANGE_EQ(it[-10], (std::vector<int>{0, 1, 2}));
    EXPECT_EQ(it - ra.begin(), 10);
}

TEST(slide_counted_mp, sized_sentinel)
{
    // radr::counted on a non-RA range is (std::counted_iterator, std::default_sentinel_t)
    auto ra  = radr::counted(lst.begin(), 4) | radr::slide_counted<2>;
    using It = radr::iterator_t<decltype(ra)>;

    EXPECT_FALSE(radr::common_range<decltype(ra)>);
    EXPECT_TRUE((std::sized_sentinel_for<std::default_sentinel_t, It>));
    EXPECT_EQ(std::ranges::distance(ra.begin(), std::default_sentinel), 3);
    EXPECT_EQ(ra.begin() - std::default_sentinel, -3);
    EXPECT_EQ(std::ranges::size(ra), 3u);
    EXPECT_EQ(flatten(ra),
              (std::vector<std::vector<int>>{
                {1, 2},
                {2, 3},
                {3, 4}
    }));
}

TEST(slide_counted_mp, const_iterator)
{
    auto       ra  = std::ref(lst) | radr::slide_counted<3>;
    auto const cra = ra;

    radr::const_iterator_t<decltype(ra)> it = ra.begin();
    EXPECT_EQ(it, cra.begin());
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 2, 3}));
}

TEST(slide_counted_mp, iterator_size)
{
    // bidirectional+common: only the begin of the window
    EXPECT_EQ(sizeof(radr::iterator_t<decltype(std::ref(lst) | radr::slide_counted<3>)>), sizeof(lst.begin()));
    EXPECT_EQ(sizeof(radr::iterator_t<decltype(std::ref(deq) | radr::slide_counted<3>)>), sizeof(deq.begin()));
    EXPECT_EQ(sizeof(radr::iterator_t<decltype(std::ref(vec) | radr::slide_counted<3>)>), sizeof(int *));

    // forward+common: begin and last of the window
    EXPECT_EQ(sizeof(radr::iterator_t<decltype(std::ref(flst) | radr::slide_counted<3>)>), 2 * sizeof(flst.begin()));

    // not common: begin and last of the window, and the underlying end
    using URange = decltype(std::ref(vec) | radr::take_while(less_four));
    EXPECT_EQ(sizeof(radr::iterator_t<decltype(std::ref(vec) | radr::take_while(less_four) | radr::slide_counted<3>)>),
              2 * sizeof(radr::iterator_t<URange>) + sizeof(radr::sentinel_t<URange>));
}

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(slide_counted_mp, deep_copy)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::slide_counted<3>);
    T cpy;
    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::slide_counted<3>;
        EXPECT_EQ(flatten(own), slides_of_3);
        cpy = own;
    }
    EXPECT_EQ(flatten(cpy), slides_of_3);
}

TEST(slide_counted_mp, deep_copy_bidi)
{
    // the end iterator is stored and must be rebound as well
    using T = decltype(std::list<int>{1, 2, 3, 4, 5} | radr::slide_counted<3>);
    T cpy;
    {
        T own = std::list<int>{1, 2, 3, 4, 5} | radr::slide_counted<3>;
        EXPECT_EQ(flatten(own), slides_of_3);
        cpy = own;
    }
    EXPECT_EQ(flatten(cpy), slides_of_3);

    auto it = cpy.end();
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3, 4, 5}));
}

TEST(slide_counted_mp, deep_copy_forward)
{
    // the last element of the window is stored and must be rebound as well
    using T = decltype(std::forward_list<int>{1, 2, 3, 4, 5} | radr::slide_counted<3>);
    T cpy;
    {
        T own = std::forward_list<int>{1, 2, 3, 4, 5} | radr::slide_counted<3>;
        EXPECT_EQ(flatten(own), slides_of_3);
        cpy = own;
    }
    EXPECT_EQ(flatten(cpy), slides_of_3);
}
