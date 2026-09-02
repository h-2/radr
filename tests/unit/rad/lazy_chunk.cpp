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
#include <radr/rad/lazy_chunk.hpp>
#include <radr/rad/take_while.hpp>
#include <radr/rad/to_single_pass.hpp>

using namespace std::string_view_literals;
using radr::test::range_cat;

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// lazy_chunk(2) of {1,2,3,4,5} -> {1,2} {3,4} {5}
inline std::vector<int>       vec{1, 2, 3, 4, 5};
inline std::list<int>         lst{1, 2, 3, 4, 5};
inline std::forward_list<int> flst{1, 2, 3, 4, 5};
inline std::deque<int>        deq{1, 2, 3, 4, 5};

inline std::vector<std::vector<int>> const chunks_of_2{
  {1, 2},
  {3, 4},
  {5}
};

// {1,2,3,5} (filtered out the 4) | lazy_chunk(2) -> {1,2} {3,5}
inline auto const not_four = [](int i)
{
    return i != 4;
};

inline auto const lt6 = [](int i)
{
    return i < 6;
};

/* The inner ranges are never common, so they cannot be used to construct a std::vector from an
 * iterator pair; they are collected element-wise instead. */
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

/* The single-pass implementation is shared with radr::chunk (radr::detail::chunk_coro), so these
 * mirror the corresponding chunk tests. */

TEST(lazy_chunk_sp, partial_last_chunk)
{
    auto ra = std::string("abcdefg") | radr::to_single_pass | radr::lazy_chunk(3);

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

TEST(lazy_chunk_sp, exact_multiple)
{
    // no phantom trailing empty chunk when size % n == 0
    auto ra = std::string("abcdef") | radr::to_single_pass | radr::lazy_chunk(3);

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

TEST(lazy_chunk_mp, forward)
{
    auto ra = std::ref(flst) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
}

TEST(lazy_chunk_mp, forward_rvalue)
{
    auto ra = std::forward_list<int>{1, 2, 3, 4, 5} | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
}

TEST(lazy_chunk_mp, bidi_common)
{
    // the outer range stays forward-only although the input is bidirectional and common
    auto ra = std::ref(lst) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .sized = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::bidi, .mut = true, .borrowed = true});
}

TEST(lazy_chunk_mp, bidi_common_rvalue)
{
    auto ra = std::list<int>{1, 2, 3, 4, 5} | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .sized = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::bidi, .mut = true, .borrowed = true});
}

TEST(lazy_chunk_mp, ra_sized)
{
    auto ra = std::ref(deq) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .sized = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::ra, .mut = true, .borrowed = true});
}

TEST(lazy_chunk_mp, ra_sized_rvalue)
{
    auto ra = std::deque<int>{1, 2, 3, 4, 5} | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .sized = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::ra, .mut = true, .borrowed = true});
}

TEST(lazy_chunk_mp, contig_sized)
{
    auto ra = std::ref(vec) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .sized = true, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::contig, .mut = true, .borrowed = true});
}

TEST(lazy_chunk_mp, contig_sized_rvalue)
{
    auto ra = std::vector<int>{1, 2, 3, 4, 5} | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .sized = true, .mut = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::contig, .mut = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

TEST(lazy_chunk_mp, mutate)
{
    std::vector<int> v{1, 2, 3, 4};
    auto             ra = std::ref(v) | radr::lazy_chunk(2);

    for (auto sub : ra)
        for (auto & x : sub)
            x += 10;

    EXPECT_RANGE_EQ(v, (std::vector<int>{11, 12, 13, 14}));
}

TEST(lazy_chunk_mp, empty)
{
    std::vector<int> v{};
    auto             ra = std::ref(v) | radr::lazy_chunk(2);

    EXPECT_TRUE(std::ranges::empty(ra));
    EXPECT_TRUE(ra.begin() == ra.end());
    EXPECT_EQ(std::ranges::size(ra), 0u);
}

TEST(lazy_chunk_mp, constant)
{
    auto ra = std::cref(vec) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .sized = true, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::contig, .constant = true, .borrowed = true});
}

TEST(lazy_chunk_mp, infinite)
{
    // unbounded radr::iota; the outer range must model radr::infinite_mp_range (std::unreachable_sentinel_t)
    auto ra = radr::iota(0) | radr::lazy_chunk(2);

    auto it = ra.begin();
    EXPECT_RANGE_EQ(*it, (std::vector<int>{0, 1}));
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{2, 3}));
    ++it;
    EXPECT_RANGE_EQ(*it, (std::vector<int>{4, 5}));
    EXPECT_FALSE(it == ra.end());

    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .infinite = true, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::ra, .constant = true, .borrowed = true});
}

TEST(lazy_chunk_mp, fwd_uncommon)
{
    // radr::filter is never common; radr::lazy_chunk needs neither commonality nor sizedness
    auto ra = std::ref(flst) | radr::filter(not_four) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra),
              (std::vector<std::vector<int>>{
                {1, 2},
                {3, 5}
    }));
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
}

TEST(lazy_chunk_mp, bidi_uncommon)
{
    auto ra = std::ref(vec) | radr::filter(not_four) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra),
              (std::vector<std::vector<int>>{
                {1, 2},
                {3, 5}
    }));
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::bidi, .constant = true, .borrowed = true});
}

TEST(lazy_chunk_mp, ra_nonsized)
{
    // contiguous but neither common nor sized; this is the configuration lazy_chunk exists for
    auto ra = std::ref(vec) | radr::take_while(lt6) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::contig, .mut = true, .borrowed = true});
}

TEST(lazy_chunk_mp, ra_nonsized_deque)
{
    // random-access but not contiguous, and neither common nor sized
    auto ra = std::ref(deq) | radr::take_while(lt6) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .mut = true, .borrowed = true});
    using val_t = std::ranges::range_value_t<decltype(ra)>;
    radr::test::check_adaptor_concepts<val_t>({.cat = range_cat::ra, .mut = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

TEST(lazy_chunk_mp, n1)
{
    auto ra = std::ref(vec) | radr::lazy_chunk(1);

    EXPECT_EQ(flatten(ra), (std::vector<std::vector<int>>{{1}, {2}, {3}, {4}, {5}}));
    EXPECT_EQ(std::ranges::size(ra), 5u);
}

TEST(lazy_chunk_mp, exact_multiple_no_trailing_empty)
{
    std::vector<int> v{1, 2, 3, 4};
    auto             ra = std::ref(v) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra),
              (std::vector<std::vector<int>>{
                {1, 2},
                {3, 4}
    }));
    EXPECT_EQ(std::ranges::distance(ra), 2);
    EXPECT_EQ(std::ranges::size(ra), 2u);
}

TEST(lazy_chunk_mp, n_larger_than_size)
{
    auto ra = std::ref(vec) | radr::lazy_chunk(100);

    EXPECT_EQ(flatten(ra), (std::vector<std::vector<int>>{vec}));
    EXPECT_EQ(std::ranges::size(ra), 1u);
}

TEST(lazy_chunk_mp, outer_size)
{
    // the outer size is the number of chunks, i.e. the underlying size rounded up
    EXPECT_EQ(std::ranges::size(std::ref(vec) | radr::lazy_chunk(2)), 3u); // 5 elements
    EXPECT_EQ(std::ranges::size(std::ref(vec) | radr::lazy_chunk(3)), 2u);
    EXPECT_EQ(std::ranges::size(std::ref(vec) | radr::lazy_chunk(5)), 1u);
    EXPECT_EQ(std::ranges::size(std::ref(lst) | radr::lazy_chunk(2)), 3u);
    EXPECT_EQ(std::ranges::size(std::ref(deq) | radr::lazy_chunk(4)), 2u);

    // ... but only when the underlying range is sized; a std::forward_list is not
    EXPECT_FALSE(std::ranges::sized_range<decltype(std::ref(flst) | radr::lazy_chunk(2))>);
}

TEST(lazy_chunk_mp, inner_range_never_sized_or_common)
{
    // in contrast to radr::chunk, this holds for every underlying range, including a std::vector
    auto check = []<typename R>(R &&)
    {
        using val_t = std::ranges::range_value_t<std::remove_cvref_t<R>>;
        EXPECT_FALSE(std::ranges::sized_range<val_t>);
        EXPECT_FALSE(radr::common_range<val_t>);
        EXPECT_TRUE(std::ranges::borrowed_range<val_t>);
    };

    check(std::ref(vec) | radr::lazy_chunk(2));
    check(std::ref(deq) | radr::lazy_chunk(2));
    check(std::ref(lst) | radr::lazy_chunk(2));
    check(std::ref(flst) | radr::lazy_chunk(2));

    // radr::chunk's inner range is sized and common for the same inputs
    using chunk_val_t = std::ranges::range_value_t<decltype(std::ref(vec) | radr::chunk(2))>;
    EXPECT_TRUE(std::ranges::sized_range<chunk_val_t>);
    EXPECT_TRUE(radr::common_range<chunk_val_t>);
}

TEST(lazy_chunk_mp, inner_range_is_counted)
{
    auto ra     = std::ref(vec) | radr::lazy_chunk(2);
    using val_t = std::ranges::range_value_t<decltype(ra)>;

    EXPECT_SAME_TYPE(radr::iterator_t<val_t>, std::counted_iterator<int *>);

    // the count is what terminates a full chunk, the underlying end what terminates the last one
    auto it = ra.begin();
    EXPECT_EQ((*it).begin().count(), 2);
    ++it;
    ++it;
    EXPECT_EQ((*it).begin().count(), 2); // the last chunk holds only one element, but still counts from n
    EXPECT_EQ(std::ranges::distance(*it), 1);
}

TEST(lazy_chunk_mp, outer_never_bidirectional)
{
    // this is the defining difference to radr::chunk: no input promotes the outer range
    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(std::ref(vec) | radr::lazy_chunk(2))>);
    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(std::ref(deq) | radr::lazy_chunk(2))>);
    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(std::ref(lst) | radr::lazy_chunk(2))>);
    EXPECT_FALSE(radr::common_range<decltype(std::ref(vec) | radr::lazy_chunk(2))>);

    // radr::chunk does promote them
    EXPECT_TRUE(std::ranges::random_access_range<decltype(std::ref(vec) | radr::chunk(2))>);
    EXPECT_TRUE(radr::common_range<decltype(std::ref(vec) | radr::chunk(2))>);
}

TEST(lazy_chunk_mp, same_chunks_as_chunk)
{
    // the results are identical to radr::chunk's for every category of input and several sizes
    for (int n : {1, 2, 3, 4, 5, 100})
    {
        EXPECT_EQ(flatten(std::ref(vec) | radr::lazy_chunk(n)), flatten(std::ref(vec) | radr::chunk(n)));
        EXPECT_EQ(flatten(std::ref(deq) | radr::lazy_chunk(n)), flatten(std::ref(deq) | radr::chunk(n)));
        EXPECT_EQ(flatten(std::ref(lst) | radr::lazy_chunk(n)), flatten(std::ref(lst) | radr::chunk(n)));
        EXPECT_EQ(flatten(std::ref(flst) | radr::lazy_chunk(n)), flatten(std::ref(flst) | radr::chunk(n)));
    }
}

TEST(lazy_chunk_mp, iterator_is_smaller_than_chunks)
{
    // not storing the chunk's end saves one underlying iterator
    auto lazy  = std::ref(flst) | radr::lazy_chunk(2);
    auto eager = std::ref(flst) | radr::chunk(2);
    EXPECT_LT(sizeof(lazy.begin()), sizeof(eager.begin()));
}

TEST(lazy_chunk_mp, multiple_passes)
{
    // forward_range: iterating twice must yield the same result and not consume anything
    auto ra = std::ref(vec) | radr::lazy_chunk(2);

    EXPECT_EQ(flatten(ra), chunks_of_2);
    EXPECT_EQ(flatten(ra), chunks_of_2);

    // and the same holds for one chunk read twice
    auto ch = *ra.begin();
    EXPECT_RANGE_EQ(ch, (std::vector<int>{1, 2}));
    EXPECT_RANGE_EQ(ch, (std::vector<int>{1, 2}));
}

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(lazy_chunk_mp, deep_copy)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::lazy_chunk(2));

    T cpy;
    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::lazy_chunk(2);
        EXPECT_EQ(flatten(own), chunks_of_2);
        cpy = own;
    }
    EXPECT_EQ(flatten(cpy), chunks_of_2);
}
