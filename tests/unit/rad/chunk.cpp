#include <deque>
#include <forward_list>
#include <list>
#include <ranges>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/iota.hpp>
#include <radr/rad/chunk.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/to_single_pass.hpp>

using namespace std::string_view_literals;
using radr::test::range_cat;

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// chunk(2) of {1,2,3,4,5} -> {1,2} {3,4} {5}
inline std::vector<int>       vec{1, 2, 3, 4, 5};
inline std::list<int>         lst{1, 2, 3, 4, 5};
inline std::forward_list<int> flst{1, 2, 3, 4, 5};
inline std::deque<int>        deq{1, 2, 3, 4, 5};

inline std::vector<std::vector<int>> const chunks_of_2{
  {1, 2},
  {3, 4},
  {5}
};

// {1,2,3,5} (filtered out the 4) | chunk(2) -> {1,2} {3,5}
inline auto const not_four = [](int i)
{
    return i != 4;
};

// radr::borrowing_rad (and radr::generator) have no cross-type operator== against std::vector,
// so range-of-range comparisons are done by first collecting into nested std::vector<int>s.
template <typename RangeOfRanges>
std::vector<std::vector<int>> flatten(RangeOfRanges && r)
{
    std::vector<std::vector<int>> out;
    for (auto && sub : r)
        out.emplace_back(sub.begin(), sub.end());
    return out;
}

// --------------------------------------------------------------------------
// single-pass tests
// --------------------------------------------------------------------------

TEST(chunk_sp, partial_last_chunk)
{
    auto ra = std::string("abcdefg") | radr::to_single_pass | radr::chunk(3);

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "abc"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "def"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "g"sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), (radr::generator<char &, char> &));
}

TEST(chunk_sp, exact_multiple)
{
    // no phantom trailing empty chunk when size % n == 0
    auto ra = std::string("abcdef") | radr::to_single_pass | radr::chunk(3);

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "abc"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "def"sv);
    ++it;
    EXPECT_EQ(it, ra.end());
}
// --------------------------------------------------------------------------
// multi-pass tests I – canonical cases
// --------------------------------------------------------------------------

TEST(chunk_mp, forward)
{
    auto ra = std::ref(flst) | radr::chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::fwd, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_mp, forward_rvalue)
{
    auto ra = std::forward_list<int>{1, 2, 3, 4, 5} | radr::chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::fwd, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_mp, bidi_common)
{
    auto ra = std::ref(lst) | radr::chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_mp, bidi_common_rvalue)
{
    auto ra = std::list<int>{1, 2, 3, 4, 5} | radr::chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_mp, ra_sized)
{
    auto ra = std::ref(deq) | radr::chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_mp, ra_sized_rvalue)
{
    auto ra = std::deque<int>{1, 2, 3, 4, 5} | radr::chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_mp, contig_sized)
{
    auto ra = std::ref(vec) | radr::chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_mp, contig_sized_rvalue)
{
    auto ra = std::vector<int>{1, 2, 3, 4, 5} | radr::chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

TEST(chunk_mp, mutate)
{
    std::vector<int> v{1, 2, 3, 4};
    auto             ra = std::ref(v) | radr::chunk(2);

    for (auto sub : ra)
        for (auto & x : sub)
            x += 10;

    EXPECT_RANGE_EQ(v, (std::vector<int>{11, 12, 13, 14}));
}

TEST(chunk_mp, empty)
{
    std::vector<int> v{};
    auto             ra = std::ref(v) | radr::chunk(2);

    EXPECT_TRUE(std::ranges::empty(ra));
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(chunk_mp, constant)
{
    auto ra = std::cref(vec) | radr::chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(chunk_mp, downgrade_infinite)
{
    // unbounded radr::iota is bidirectional but neither common nor sized -> downgrades; the outer
    // range must still model radr::infinite_mp_range via the downgrade path (std::unreachable_sentinel_t)
    auto ra = radr::iota(0) | radr::chunk(2);

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, (std::vector<int>{0, 1}));
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2, 3}));

    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .infinite = true, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(chunk_mp, downgrade_not_bidirectional)
{
    auto ra = std::ref(flst) | radr::chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    EXPECT_TRUE((std::same_as<decltype(ra), decltype(std::ref(flst) | radr::chunk(2))>));
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::fwd, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_mp, downgrade_uncommon)
{
    // radr::filter is bidirectional but never common -> downgrades, even though bidirectional_range
    // alone would have been satisfied.
    auto ra = std::ref(vec) | radr::filter(not_four) | radr::chunk(2);

    EXPECT_EQ(flatten(ra),
              (std::vector<std::vector<int>>{
                {1, 2},
                {3, 5}
    }));
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .constant = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

TEST(chunk_mp, n1)
{
    // every element its own chunk
    auto ra = std::ref(vec) | radr::chunk(1);

    EXPECT_EQ(flatten(ra), (std::vector<std::vector<int>>{{1}, {2}, {3}, {4}, {5}}));
}

TEST(chunk_mp, exact_multiple_no_trailing_empty)
{
    std::vector<int> v{1, 2, 3, 4};
    auto             ra = std::ref(v) | radr::chunk(2);

    EXPECT_EQ(flatten(ra),
              (std::vector<std::vector<int>>{
                {1, 2},
                {3, 4}
    }));
    EXPECT_EQ(std::ranges::distance(ra), 2);
}

TEST(chunk_mp, random_access_operations)
{
    // size 7, n 3 -> {1,2,3} {4,5,6} {7}; exercises ra_chunk_like_iterator's operator[]/+/-/jump,
    // including jumping directly from end() into the short last chunk (the case that a naive
    // "extrapolate from the current position" design gets wrong, see the design discussion)
    std::vector<int> v{1, 2, 3, 4, 5, 6, 7};
    auto             ra = std::ref(v) | radr::chunk(3);

    EXPECT_EQ(std::ranges::size(ra), 3u);
    EXPECT_EQ(ra.end() - ra.begin(), 3);

    EXPECT_RANGE_EQ(ra[0], (std::vector<int>{1, 2, 3}));
    EXPECT_RANGE_EQ(ra[1], (std::vector<int>{4, 5, 6}));
    EXPECT_RANGE_EQ(ra[2], (std::vector<int>{7}));

    auto it = ra.begin() + 2;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{7}));
    EXPECT_TRUE(it == ra.end() - 1);

    // jump directly from end(), not via repeated --
    EXPECT_TRUE(ra.end() + (-3) == ra.begin());

    EXPECT_TRUE(ra.begin() < ra.end());
    EXPECT_TRUE(ra.begin() <= ra.begin());
    EXPECT_FALSE(ra.begin() > ra.end());
}

TEST(chunk_mp, n_larger_than_size)
{
    // one chunk containing everything; decrementing from end() must land directly on begin()
    std::vector<int> v{1, 2, 3, 4, 5};
    auto             ra = std::ref(v) | radr::chunk(100);

    EXPECT_EQ(flatten(ra), (std::vector<std::vector<int>>{v}));

    auto it = ra.end();
    --it;
    EXPECT_RANGE_EQ(*it, v);
    EXPECT_TRUE(it == ra.begin());
}

TEST(chunk_mp, inner_size_non_sized_sentinel)
{
    // std::list's iterator is no std::sized_sentinel_for itself, so the inner range's size comes from the
    // boundary finder's n -- except for the last chunk, which is counted because it may be shorter
    std::vector<size_t> sizes;
    for (auto && chunk : std::ref(lst) | radr::chunk(2))
        sizes.push_back(std::ranges::size(chunk));

    EXPECT_EQ(sizes, (std::vector<size_t>{2, 2, 1}));

    // the same for a forward_list, i.e. via unidi_chunk_like_iterator
    sizes.clear();
    for (auto && chunk : std::ref(flst) | radr::chunk(2))
        sizes.push_back(std::ranges::size(chunk));

    EXPECT_EQ(sizes, (std::vector<size_t>{2, 2, 1}));
}

TEST(chunk_mp, reverse_iteration_short_last_chunk)
{
    // size 5, n 2 -> {1,2} {3,4} {5}; decrementing from end() must reproduce this in reverse
    std::vector<int> v{1, 2, 3, 4, 5};
    auto             ra = std::ref(v) | radr::chunk(2);

    auto it = ra.end();
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{5}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3, 4}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 2}));
    EXPECT_TRUE(it == ra.begin());
}

TEST(chunk_mp, reverse_iteration_exact_multiple)
{
    // size 4, n 2 -> {1,2} {3,4}; no phantom empty chunk when decrementing from end() either
    std::vector<int> v{1, 2, 3, 4};
    auto             ra = std::ref(v) | radr::chunk(2);

    auto it = ra.end();
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3, 4}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 2}));
    EXPECT_TRUE(it == ra.begin());
}

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(chunk_mp, deep_copy)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::chunk(2));
    T cpy;
    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::chunk(2);
        EXPECT_EQ(flatten(own), chunks_of_2);
        cpy = own;
    }
    EXPECT_EQ(flatten(cpy), chunks_of_2);
}
