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
#include <radr/rad/chunk.hpp>
#include <radr/rad/chunk_counted.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/take_while.hpp>

using namespace std::string_view_literals;
using radr::test::range_cat;

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// chunk_counted<2> of {1,2,3,4,5} -> {1,2} {3,4} {5}
inline std::vector<int>       vec{1, 2, 3, 4, 5};
inline std::deque<int>        deq{1, 2, 3, 4, 5};
inline std::list<int>         lst{1, 2, 3, 4, 5};
inline std::forward_list<int> flst{1, 2, 3, 4, 5};

inline std::vector<std::vector<int>> const chunks_of_2{
  {1, 2},
  {3, 4},
  {5}
};

// {1,2,3,5} (filtered out the 4) | chunk_counted<2> -> {1,2} {3,5}
inline auto const not_four = [](int i)
{
    return i != 4;
};

inline std::vector<std::vector<int>> const filtered_chunks_of_2{
  {1, 2},
  {3, 5}
};

// {1,2,3} (take_while < 4) | chunk_counted<2> -> {1,2} {3}
inline auto const less_four = [](int i)
{
    return i < 4;
};

inline std::vector<std::vector<int>> const taken_chunks_of_2{
  {1, 2},
  {3}
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
// multi-pass tests I – canonical cases
// --------------------------------------------------------------------------

TEST(chunk_counted_mp, forward)
{
    auto ra = std::ref(flst) | radr::chunk_counted<2>;

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::fwd, .sized = true, .mut = true, .borrowed = true});
}

TEST(chunk_counted_mp, bidi_common)
{
    auto ra = std::ref(lst) | radr::chunk_counted<2>;

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::bidi, .sized = true, .mut = true, .borrowed = true});
}

TEST(chunk_counted_mp, ra_sized)
{
    auto ra = std::ref(deq) | radr::chunk_counted<2>;

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});

    using val_t = std::ranges::range_value_t<decltype(ra)>;
    EXPECT_SAME_TYPE(std::ranges::iterator_t<val_t>, std::counted_iterator<std::deque<int>::iterator>);
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_counted_mp, contig_sized)
{
    auto ra = std::ref(vec) | radr::chunk_counted<2>;

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
    // the chunks are counted ranges, also for contiguous ranges
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    EXPECT_SAME_TYPE(std::ranges::iterator_t<val_t>, std::counted_iterator<int *>);
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_counted_mp, contig_sized_rvalue)
{
    auto ra = std::vector<int>{1, 2, 3, 4, 5} | radr::chunk_counted<2>;

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true});
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

TEST(chunk_counted_mp, empty)
{
    std::vector<int> v{};
    std::list<int>   l{};

    EXPECT_TRUE(std::ranges::empty(std::ref(v) | radr::chunk_counted<2>));
    EXPECT_EQ(std::ranges::size(std::ref(v) | radr::chunk_counted<2>), 0u);
    EXPECT_TRUE(std::ranges::empty(std::ref(l) | radr::chunk_counted<2>));
    EXPECT_TRUE(std::ranges::empty(std::forward_list<int>{} | radr::chunk_counted<2>));
}

TEST(chunk_counted_mp, mutate)
{
    std::list<int> l{1, 2, 3, 4, 5};
    for (auto sub : std::ref(l) | radr::chunk_counted<2>)
        for (int & i : sub)
            i += 10;

    EXPECT_RANGE_EQ(l, (std::vector<int>{11, 12, 13, 14, 15}));
}

TEST(chunk_counted_mp, infinite)
{
    auto ra = radr::iota(0) | radr::chunk_counted<2>;

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, (std::vector<int>{0, 1}));
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2, 3}));

    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .infinite = true, .constant = true, .borrowed = true});
}

TEST(chunk_counted_mp, fwd_uncommon)
{
    auto ra = std::ref(flst) | radr::filter(not_four) | radr::chunk_counted<2>;

    EXPECT_EQ(flatten(ra), filtered_chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
}

TEST(chunk_counted_mp, contig_nonsized)
{
    auto ra = std::ref(vec) | radr::take_while(less_four) | radr::chunk_counted<2>;

    EXPECT_EQ(flatten(ra), taken_chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_counted_mp, sized_uncommon)
{
    // radr::counted on a non-RA range is (std::counted_iterator, std::default_sentinel_t)
    auto ra  = radr::counted(lst.begin(), 5) | radr::chunk_counted<2>;
    using It = radr::iterator_t<decltype(ra)>;

    EXPECT_FALSE(radr::common_range<decltype(ra)>);
    EXPECT_TRUE((std::sized_sentinel_for<std::default_sentinel_t, It>));
    EXPECT_EQ(std::ranges::size(ra), 3u);
    EXPECT_EQ(std::ranges::distance(ra.begin(), std::default_sentinel), 3);
    EXPECT_EQ(flatten(ra), chunks_of_2);
}

// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

TEST(chunk_counted_mp, same_chunks_as_chunk)
{
    auto check = [](auto && urange)
    {
        EXPECT_EQ(flatten(urange | radr::chunk_counted<1>), flatten(urange | radr::chunk(1)));
        EXPECT_EQ(flatten(urange | radr::chunk_counted<2>), flatten(urange | radr::chunk(2)));
        EXPECT_EQ(flatten(urange | radr::chunk_counted<3>), flatten(urange | radr::chunk(3)));
        EXPECT_EQ(flatten(urange | radr::chunk_counted<5>), flatten(urange | radr::chunk(5)));
        EXPECT_EQ(flatten(urange | radr::chunk_counted<6>), flatten(urange | radr::chunk(6)));
    };

    check(std::ref(vec));
    check(std::ref(deq));
    check(std::ref(lst));
    check(std::ref(flst));
    check(std::ref(flst) | radr::filter(not_four));
    check(std::ref(vec) | radr::take_while(less_four));
}

TEST(chunk_counted_mp, size)
{
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::chunk_counted<1>), 5u);
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::chunk_counted<2>), 3u);
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::chunk_counted<5>), 1u);
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::chunk_counted<6>), 1u);
    EXPECT_EQ(std::ranges::size(std::ref(deq) | radr::chunk_counted<4>), 2u);
    EXPECT_EQ(std::ranges::size(std::ref(vec) | radr::chunk_counted<5>), 1u);
}

TEST(chunk_counted_mp, inner_size)
{
    // the size of the inner range is computed from the count
    auto ra = "abcde"sv | radr::chunk_counted<2>;
    auto it = ra.begin();

    EXPECT_EQ(std::ranges::size(*it), 2u);
    EXPECT_RANGE_EQ(*it, "ab"sv);
    it += 2;
    EXPECT_EQ(std::ranges::size(*it), 1u);
    EXPECT_RANGE_EQ(*it, "e"sv);
    EXPECT_EQ(std::ranges::distance((*it).begin(), (*it).end()), 1);

    // unsized underlying range
    auto ra2 = std::ref(flst) | radr::chunk_counted<3>;
    auto it2 = ra2.begin();
    EXPECT_EQ(std::ranges::size(*it2), 3u);
    ++it2;
    EXPECT_EQ(std::ranges::size(*it2), 2u);
    EXPECT_RANGE_EQ(*it2, (std::vector<int>{4, 5}));
}

TEST(chunk_counted_mp, reverse_iteration)
{
    auto check = [](auto ra)
    {
        auto it = ra.end();
        --it;
        EXPECT_RANGE_EQ(*it, (std::vector<int>{5}));
        --it;
        EXPECT_RANGE_EQ(*it, (std::vector<int>{3, 4}));
        it--;
        EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 2}));
        EXPECT_EQ(it, ra.begin());
    };

    check(std::ref(lst) | radr::chunk_counted<2>);
    check(std::ref(deq) | radr::chunk_counted<2>);
    check(std::ref(vec) | radr::chunk_counted<2>);

    // exact multiple
    auto ra = std::ref(lst) | radr::chunk_counted<5>;
    auto it = ra.end();
    --it;
    EXPECT_RANGE_EQ(*it, lst);
    EXPECT_EQ(it, ra.begin());
}

TEST(chunk_counted_mp, random_access_operations)
{
    auto check = [](auto ra)
    {
        auto b = ra.begin();
        auto e = ra.end();
        EXPECT_EQ(e - b, 3);
        EXPECT_EQ(b - e, -3);
        EXPECT_RANGE_EQ(b[2], (std::vector<int>{5}));
        EXPECT_RANGE_EQ(*(b + 1), (std::vector<int>{3, 4}));
        EXPECT_RANGE_EQ(*(1 + b), (std::vector<int>{3, 4}));
        EXPECT_RANGE_EQ(*(e - 3), (std::vector<int>{1, 2}));
        EXPECT_RANGE_EQ(*(e - 1), (std::vector<int>{5}));
        EXPECT_EQ(b + 3, e);
        EXPECT_LT(b, e);
        EXPECT_GT(e, b + 2);

        auto it = b;
        it += 2;
        EXPECT_RANGE_EQ(*it, (std::vector<int>{5}));
        it -= 2;
        EXPECT_EQ(it, b);
    };

    check(std::ref(vec) | radr::chunk_counted<2>);
    check(std::ref(deq) | radr::chunk_counted<2>);
}

TEST(chunk_counted_mp, const_iterator)
{
    auto       ra  = std::ref(lst) | radr::chunk_counted<2>;
    auto const cra = ra;

    radr::const_iterator_t<decltype(ra)> it = ra.begin();
    EXPECT_EQ(it, cra.begin());
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 2}));
}

TEST(chunk_counted_mp, iterator_size)
{
    // sized: one underlying iterator and two integers
    EXPECT_EQ(sizeof(radr::iterator_t<decltype(std::ref(vec) | radr::chunk_counted<2>)>),
              sizeof(int *) + 2 * sizeof(ptrdiff_t));
    EXPECT_EQ(sizeof(radr::iterator_t<decltype(std::ref(lst) | radr::chunk_counted<2>)>),
              sizeof(lst.begin()) + 2 * sizeof(ptrdiff_t));

    // unsized: underlying end, begin and end of the chunk, and its length
    EXPECT_EQ(sizeof(radr::iterator_t<decltype(std::ref(flst) | radr::chunk_counted<2>)>),
              3 * sizeof(flst.begin()) + sizeof(ptrdiff_t));
}

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(chunk_counted_mp, deep_copy)
{
    auto check = []<typename T>(T && init)
    {
        using V = decltype(std::forward<T>(init) | radr::chunk_counted<2>);
        V cpy;
        {
            V own = T{1, 2, 3, 4, 5} | radr::chunk_counted<2>;
            EXPECT_EQ(flatten(own), chunks_of_2);
            cpy = own;
        }
        EXPECT_EQ(flatten(cpy), chunks_of_2);
    };

    check(std::vector<int>{});
    check(std::deque<int>{});
    check(std::list<int>{});
    check(std::forward_list<int>{});
}
