#include <deque>
#include <forward_list>
#include <list>
#include <ranges>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/counted.hpp>
#include <radr/factory/iota.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/slide.hpp>
#include <radr/rad/take_while.hpp>
#include <radr/rad/to_single_pass.hpp>

using radr::test::range_cat;

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// slide(3) of {1,2,3,4,5} -> {1,2,3} {2,3,4} {3,4,5}
inline std::vector<int>       vec{1, 2, 3, 4, 5};
inline std::deque<int>        deq{1, 2, 3, 4, 5};
inline std::list<int>         lst{1, 2, 3, 4, 5};
inline std::forward_list<int> flst{1, 2, 3, 4, 5};

inline std::vector<std::vector<int>> const slides_of_3{
  {1, 2, 3},
  {2, 3, 4},
  {3, 4, 5}
};

// {1,2,3,5} (filtered out the 4) | slide(2) -> {1,2} {2,3} {3,5}
inline auto const not_four = [](int i)
{
    return i != 4;
};

inline std::vector<std::vector<int>> const filtered_slides_of_2{
  {1, 2},
  {2, 3},
  {3, 5}
};

// {1,2,3} (take_while < 4) | slide(2) -> {1,2} {2,3}
inline auto const less_four = [](int i)
{
    return i < 4;
};

inline std::vector<std::vector<int>> const taken_slides_of_2{
  {1, 2},
  {2, 3}
};

// radr::borrowing_rad (and radr::generator) have no cross-type operator== against std::vector,
// so range-of-range comparisons are done by first collecting into nested std::vector<int>s.
template <typename T = int, typename RangeOfRanges>
std::vector<std::vector<T>> materialise(RangeOfRanges && r)
{
    std::vector<std::vector<T>> out;
    for (auto && sub : r)
    {
        out.emplace_back();
        for (auto && elem : sub)
            out.back().push_back(elem);
    }
    return out;
}

// --------------------------------------------------------------------------
// single-pass tests
// --------------------------------------------------------------------------

// no single-pass version

// --------------------------------------------------------------------------
// multi-pass tests I – canonical cases
// --------------------------------------------------------------------------

TEST(slide_mp, forward)
{
    auto ra = std::ref(flst) | radr::slide(3);

    EXPECT_EQ(materialise(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::fwd, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_mp, forward_rvalue)
{
    auto ra = std::forward_list<int>{1, 2, 3, 4, 5} | radr::slide(3);

    EXPECT_EQ(materialise(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::fwd, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_mp, bidi_common)
{
    auto ra = std::ref(lst) | radr::slide(3);

    EXPECT_EQ(materialise(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_mp, bidi_common_rvalue)
{
    auto ra = std::list<int>{1, 2, 3, 4, 5} | radr::slide(3);

    EXPECT_EQ(materialise(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_mp, ra_sized)
{
    auto ra = std::ref(deq) | radr::slide(3);

    EXPECT_EQ(materialise(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_mp, ra_sized_rvalue)
{
    auto ra = std::deque<int>{1, 2, 3, 4, 5} | radr::slide(3);

    EXPECT_EQ(materialise(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_mp, contig_sized)
{
    auto ra = std::ref(vec) | radr::slide(3);

    EXPECT_EQ(materialise(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_mp, contig_sized_rvalue)
{
    auto ra = std::vector<int>{1, 2, 3, 4, 5} | radr::slide(3);

    EXPECT_EQ(materialise(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

TEST(slide_mp, mutate)
{
    std::vector<int> v{1, 2, 3, 4};
    auto             ra = std::ref(v) | radr::slide(2);

    // windows overlap, so modify only the first element of each window
    for (auto sub : ra)
        *sub.begin() += 10;

    EXPECT_RANGE_EQ(v, (std::vector<int>{11, 12, 13, 4}));
}

TEST(slide_mp, empty)
{
    std::vector<int> v{};
    auto             ra = std::ref(v) | radr::slide(2);

    EXPECT_TRUE(std::ranges::empty(ra));
    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(slide_mp, constant)
{
    auto ra = std::cref(vec) | radr::slide(3);

    EXPECT_EQ(materialise(ra), slides_of_3);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(slide_mp, infinite)
{
    auto ra = radr::iota(0) | radr::slide(2);

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

TEST(slide_mp, fwd_uncommon)
{
    auto ra = std::ref(flst) | radr::filter(not_four) | radr::slide(2);

    EXPECT_EQ(materialise(ra), filtered_slides_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::fwd, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(slide_mp, bidi_uncommon)
{
    auto ra = std::ref(vec) | radr::filter(not_four) | radr::slide(2);

    EXPECT_EQ(materialise(ra), filtered_slides_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(slide_mp, ra_nonsized)
{
    auto ra = std::ref(deq) | radr::take_while(less_four) | radr::slide(2);

    EXPECT_EQ(materialise(ra), taken_slides_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(slide_mp, contig_nonsized)
{
    auto ra = std::ref(vec) | radr::take_while(less_four) | radr::slide(2);

    EXPECT_EQ(materialise(ra), taken_slides_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

TEST(slide_mp, n1)
{
    auto ra = std::ref(lst) | radr::slide(1);
    EXPECT_EQ(materialise(ra), (std::vector<std::vector<int>>{{1}, {2}, {3}, {4}, {5}}));
    EXPECT_EQ(std::ranges::size(ra), 5u);
}

TEST(slide_mp, n_equals_size)
{
    auto expected = std::vector<std::vector<int>>{
      {1, 2, 3, 4, 5}
    };
    EXPECT_EQ(materialise(std::ref(flst) | radr::slide(5)), expected);
    EXPECT_EQ(materialise(std::ref(lst) | radr::slide(5)), expected);
    EXPECT_EQ(materialise(std::ref(vec) | radr::slide(5)), expected);
    EXPECT_EQ(materialise(std::ref(vec) | radr::filter([](int) { return true; }) | radr::slide(5)), expected);
}

TEST(slide_mp, shorter_than_n)
{
    // forward + common
    EXPECT_TRUE(std::ranges::empty(std::ref(flst) | radr::slide(6)));
    EXPECT_TRUE(std::ranges::empty(std::ref(flst) | radr::slide(7)));
    // bidi + common
    EXPECT_TRUE(std::ranges::empty(std::ref(lst) | radr::slide(6)));
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::slide(6)), 0u);
    // ra + sized
    EXPECT_TRUE(std::ranges::empty(std::ref(vec) | radr::slide(6)));
    EXPECT_EQ(std::ranges::size(std::ref(vec) | radr::slide(6)), 0u);
    // uncommon
    EXPECT_TRUE(std::ranges::empty(std::ref(flst) | radr::filter(not_four) | radr::slide(5)));
    EXPECT_TRUE(std::ranges::empty(std::ref(vec) | radr::take_while(less_four) | radr::slide(4)));
}

TEST(slide_mp, size)
{
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::slide(1)), 5u);
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::slide(3)), 3u);
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::slide(5)), 1u);
    EXPECT_EQ(std::ranges::size(std::ref(deq) | radr::slide(4)), 2u);
}

TEST(slide_mp, n_type)
{
    // the type of n is stored in the iterator
    auto ra = std::ref(lst) | radr::slide(int8_t{3});
    EXPECT_EQ(materialise(ra), slides_of_3);
    EXPECT_EQ(std::ranges::size(ra), 3u);
}

TEST(slide_mp, reverse_iteration)
{
    auto ra = std::ref(lst) | radr::slide(3);

    auto it = ra.end();
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3, 4, 5}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2, 3, 4}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(it, ra.begin());
}

TEST(slide_mp, reverse_iteration_uncommon)
{
    // no decrement from the end possible, but from any valid position
    auto ra = std::ref(vec) | radr::filter(not_four) | radr::slide(2);

    auto it = std::ranges::next(ra.begin(), 2);
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3, 5}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2, 3}));
    --it;
    EXPECT_EQ(it, ra.begin());
}

TEST(slide_mp, random_access_operations)
{
    auto ra = std::ref(vec) | radr::slide(3);

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

TEST(slide_mp, random_access_operations_uncommon)
{
    auto ra = std::ref(vec) | radr::take_while(less_four) | radr::slide(2);

    auto b  = ra.begin();
    auto it = b + 1;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2, 3}));
    EXPECT_RANGE_EQ(it[-1], (std::vector<int>{1, 2}));
    EXPECT_EQ(it - b, 1);
    EXPECT_LT(b, it);
    EXPECT_EQ(std::ranges::next(it), ra.end());
}

TEST(slide_mp, random_access_operations_infinite)
{
    auto ra = radr::iota(0) | radr::slide(3);

    auto it = ra.begin() + 10;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{10, 11, 12}));
    EXPECT_RANGE_EQ(it[-10], (std::vector<int>{0, 1, 2}));
    EXPECT_EQ(it - ra.begin(), 10);
}

TEST(slide_mp, sized_sentinel)
{
    // radr::counted on a non-RA range is (std::counted_iterator, std::default_sentinel_t)
    auto ra  = radr::counted(lst.begin(), 4) | radr::slide(2);
    using It = radr::iterator_t<decltype(ra)>;

    EXPECT_FALSE(radr::common_range<decltype(ra)>);
    EXPECT_TRUE((std::sized_sentinel_for<std::default_sentinel_t, It>));
    EXPECT_EQ(std::ranges::distance(ra.begin(), std::default_sentinel), 3);
    EXPECT_EQ(std::ranges::size(ra), 3u);
    EXPECT_EQ(materialise(ra),
              (std::vector<std::vector<int>>{
                {1, 2},
                {2, 3},
                {3, 4}
    }));
}

TEST(slide_mp, const_iterator)
{
    auto       ra  = std::ref(lst) | radr::slide(3);
    auto const cra = ra;

    radr::const_iterator_t<decltype(ra)> it = ra.begin();
    EXPECT_EQ(it, cra.begin());
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 2, 3}));
}

TEST(slide_mp, iterator_size)
{
    // uend is only stored if the underlying range is not common
    auto ra = std::ref(lst) | radr::slide(std::ptrdiff_t{3});
    EXPECT_EQ(sizeof(ra.begin()), 2 * sizeof(lst.begin()) + sizeof(std::ptrdiff_t));

    auto ra2 = std::ref(flst) | radr::filter(not_four) | radr::slide(std::ptrdiff_t{3});
    EXPECT_GT(sizeof(ra2.begin()), 2 * sizeof(radr::iterator_t<decltype(std::ref(flst) | radr::filter(not_four))>));
}

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(slide_mp, deep_copy)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::slide(3));
    T cpy;
    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::slide(3);
        EXPECT_EQ(materialise(own), slides_of_3);
        cpy = own;
    }
    EXPECT_EQ(materialise(cpy), slides_of_3);
}

TEST(slide_mp, deep_copy_bidi)
{
    // the end iterator is stored and must be rebound as well
    using T = decltype(std::list<int>{1, 2, 3, 4, 5} | radr::slide(3));
    T cpy;
    {
        T own = std::list<int>{1, 2, 3, 4, 5} | radr::slide(3);
        cpy   = own;
    }
    EXPECT_EQ(materialise(cpy), slides_of_3);

    auto it = cpy.end();
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3, 4, 5}));
}
