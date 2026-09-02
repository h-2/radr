#include <deque>
#include <forward_list>
#include <functional>
#include <list>
#include <ranges>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/iota.hpp>
#include <radr/rad/chunk_by.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/take_while.hpp>
#include <radr/rad/to_single_pass.hpp>

using namespace std::string_view_literals;
using radr::test::range_cat;

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// group consecutive equal elements: {1,1,2,2,2,3,1,1} -> {1,1} {2,2,2} {3} {1,1}
inline std::vector<int>       vec{1, 1, 2, 2, 2, 3, 1, 1};
inline std::list<int>         lst{1, 1, 2, 2, 2, 3, 1, 1};
inline std::forward_list<int> flst{1, 1, 2, 2, 2, 3, 1, 1};
inline std::deque<int>        deq{1, 1, 2, 2, 2, 3, 1, 1};

inline std::vector<std::vector<int>> const grouped{
  {1, 1},
  {2, 2, 2},
  {3},
  {1, 1}
};

inline auto const lt6 = [](int i)
{
    return i < 6;
};

inline auto const eq = std::equal_to<>{};

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

TEST(chunk_by_sp, simple)
{
    auto ra = std::string("aabccc") | radr::to_single_pass | radr::chunk_by(eq);

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "aa"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "b"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "ccc"sv);
    ++it;
    EXPECT_EQ(it, ra.end());

    EXPECT_SAME_TYPE(decltype(*it), (radr::generator<char &, char> &));
}

TEST(chunk_by_sp, no_boundary_ever_matches)
{
    // always-false predicate: every element its own chunk, no phantom trailing empty chunk
    auto ra = std::string("abc") | radr::to_single_pass | radr::chunk_by([](char, char) { return false; });

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, "a"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "b"sv);
    ++it;
    EXPECT_RANGE_EQ(*it, "c"sv);
    ++it;
    EXPECT_EQ(it, ra.end());
}

// --------------------------------------------------------------------------
// multi-pass tests I – canonical cases
// --------------------------------------------------------------------------

TEST(chunk_by_mp, forward)
{
    auto ra = std::ref(flst) | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::fwd, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_by_mp, forward_rvalue)
{
    auto ra = std::forward_list<int>{1, 1, 2, 2, 2, 3, 1, 1} | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::fwd, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_by_mp, bidi_common)
{
    auto ra = std::ref(lst) | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped);
    // bidirectional_range + common_range are preserved when the input models both (regardless of
    // sizedness/random-access); the outer range is never sized or random-access, though (see docs).
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::bidi, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_by_mp, bidi_common_rvalue)
{
    auto ra = std::list<int>{1, 1, 2, 2, 2, 3, 1, 1} | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::bidi, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_by_mp, ra_sized)
{
    auto ra = std::ref(deq) | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped);
    // outer is capped at bidirectional_range even though the input is random-access+sized (see docs)
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .common = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_by_mp, ra_sized_rvalue)
{
    auto ra = std::deque<int>{1, 1, 2, 2, 2, 3, 1, 1} | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_by_mp, contig_sized)
{
    auto ra = std::ref(vec) | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped);
    // outer is capped at bidirectional_range even though the input is contiguous+sized (see docs)
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .common = true, .mut = true, .borrowed = true});
    // inner chunks are still contiguous+sized, i.e. radr::subborrow of a vector's iterator
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, radr::borrowing_rad<int *>);
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_by_mp, contig_sized_rvalue)
{
    auto ra = std::vector<int>{1, 1, 2, 2, 2, 3, 1, 1} | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .common = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

TEST(chunk_by_mp, mutate)
{
    std::vector<int> v{1, 1, 2, 2};
    auto             ra = std::ref(v) | radr::chunk_by(eq);

    for (auto sub : ra)
        for (auto & x : sub)
            x += 10;

    EXPECT_RANGE_EQ(v, (std::vector<int>{11, 11, 12, 12}));
}

TEST(chunk_by_mp, empty)
{
    std::vector<int> v{};
    auto             ra = std::ref(v) | radr::chunk_by(eq);

    EXPECT_TRUE(std::ranges::empty(ra));
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(chunk_by_mp, constant)
{
    auto ra = std::cref(vec) | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped);
    // outer is capped at bidirectional_range even though the input is contiguous+sized (see docs)
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .common = true, .constant = true, .borrowed = true});
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(*ra.begin())>, int const &);
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(chunk_by_mp, infinite)
{
    // radr::iota(0) is strictly ascending -> every element is its own chunk under std::equal_to
    auto ra = radr::iota(0) | radr::chunk_by(eq);

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, (std::vector<int>{0}));
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1}));

    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .infinite = true, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(chunk_by_mp, fwd_uncommon)
{
    // {1,1,2,2,2,3,1,1} | filter(!=3) -> {1,1,2,2,2,1,1} | chunk_by(eq) -> {1,1} {2,2,2} {1,1}
    auto ra = std::ref(flst) | radr::filter([](int i) { return i != 3; }) | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra),
              (std::vector<std::vector<int>>{
                {1, 1},
                {2, 2, 2},
                {1, 1}
    }));
    // radr::filter is a constant range, hence .mut == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::fwd, .common = true, .constant = true, .borrowed = true});
}

TEST(chunk_by_mp, bidi_uncommon)
{
    auto ra = std::ref(vec) | radr::filter([](int i) { return i != 3; }) | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra),
              (std::vector<std::vector<int>>{
                {1, 1},
                {2, 2, 2},
                {1, 1}
    }));
    // radr::filter is a constant range, hence .mut == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::bidi, .common = true, .constant = true, .borrowed = true});
}

TEST(chunk_by_mp, ra_nonsized)
{
    auto ra = std::ref(vec) | radr::take_while(lt6) | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped); // all elements of vec are < 6
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::contig, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(chunk_by_mp, contig_nonsized)
{
    auto ra = std::ref(deq) | radr::take_while(lt6) | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra), grouped);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}
// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

TEST(chunk_by_mp, always_false_predicate)
{
    // every element its own chunk, no phantom trailing empty chunk
    auto ra = std::ref(vec) | radr::chunk_by([](int, int) { return false; });

    EXPECT_EQ(flatten(ra), (std::vector<std::vector<int>>{{1}, {1}, {2}, {2}, {2}, {3}, {1}, {1}}));
}

TEST(chunk_by_mp, ascending_runs)
{
    std::vector<int> v{1, 2, 3, 1, 5, 2, 3};
    auto             ra = std::ref(v) | radr::chunk_by(std::less<>{});

    EXPECT_EQ(flatten(ra),
              (std::vector<std::vector<int>>{
                {1, 2, 3},
                {1, 5},
                {2, 3}
    }));
}

TEST(chunk_by_mp, boundary_at_very_end)
{
    // a boundary landing exactly on the last element must not produce a phantom trailing empty chunk
    std::vector<int> v{1, 1, 2};
    auto             ra = std::ref(v) | radr::chunk_by(eq);

    EXPECT_EQ(flatten(ra),
              (std::vector<std::vector<int>>{
                {1, 1},
                {2}
    }));
    EXPECT_EQ(std::ranges::distance(ra), 2);
}

TEST(chunk_by_mp, always_true_predicate)
{
    // a single chunk containing everything; decrementing from end() must land directly on begin()
    auto ra = std::ref(vec) | radr::chunk_by([](int, int) { return true; });

    EXPECT_EQ(flatten(ra), (std::vector<std::vector<int>>{vec}));

    auto it = ra.end();
    --it;
    EXPECT_RANGE_EQ(*it, vec);
    EXPECT_TRUE(it == ra.begin());
}

TEST(chunk_by_mp, reverse_iteration)
{
    // {1,1,2,2,2,3,1,1} -> {1,1} {2,2,2} {3} {1,1}; decrementing from end() must reproduce this
    // in reverse, including correctly finding the start of the last chunk without any special-casing
    // (unlike fixed-size chunk, chunk_by needs none, see the file-level comment above)
    std::vector<int> v{1, 1, 2, 2, 2, 3, 1, 1};
    auto             ra = std::ref(v) | radr::chunk_by(eq);

    auto it = ra.end();
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 1}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2, 2, 2}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1, 1}));
    EXPECT_TRUE(it == ra.begin());
}

TEST(chunk_by_mp, reverse_iteration_no_boundary_ever_matches)
{
    // always-false predicate: every element its own chunk, no phantom trailing empty chunk either way
    std::vector<int> v{1, 2, 3};
    auto             ra = std::ref(v) | radr::chunk_by([](int, int) { return false; });

    auto it = ra.end();
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{3}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2}));
    --it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{1}));
    EXPECT_TRUE(it == ra.begin());
}

TEST(chunk_by_mp, reverse_iteration_empty)
{
    std::vector<int> v{};
    auto             ra = std::ref(v) | radr::chunk_by(eq);

    EXPECT_TRUE(ra.begin() == ra.end());
}

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(chunk_by_mp, deep_copy)
{
    using T = decltype(std::vector<int>{1, 1, 2, 2, 2, 3, 1, 1} | radr::chunk_by(eq));
    T cpy;
    {
        T own = std::vector<int>{1, 1, 2, 2, 2, 3, 1, 1} | radr::chunk_by(eq);
        EXPECT_EQ(flatten(own), grouped);
        cpy = own;
    }
    EXPECT_EQ(flatten(cpy), grouped);
}
