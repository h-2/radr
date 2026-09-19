// Test-suite for radr::adjacent_transform, following UNIT_TEST_TEMPLATE.cxx.
// NOTE: deliberately NOT guarded by RADR_FEATURE_ZIP; radr::adjacent_transform is available in C++20.

#include <deque>
#include <forward_list>
#include <functional>
#include <list>
#include <ranges>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/iota.hpp>
#include <radr/rad/adjacent_transform.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/take.hpp>
#include <radr/rad/take_while.hpp>

using radr::test::range_cat;

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// non-const, but only ever read (mutation tests use their own locals)
inline std::vector<int>       vec{1, 2, 3, 4, 5};
inline std::list<int>         lst{1, 2, 3, 4, 5};
inline std::forward_list<int> flst{1, 2, 3, 4, 5};
inline std::deque<int>        deq{1, 2, 3, 4, 5};

inline constexpr auto add = [](int a, int b)
{
    return a + b;
};

inline constexpr auto add3 = [](int a, int b, int c)
{
    return a + b + c;
};

// returns a reference to the first element of the window; generic so that it also accepts the const iterators
inline constexpr auto first_ref = [](auto & a, auto &) -> auto &
{
    return a;
};

// pairwise sums of {1,2,3,4,5}
inline std::vector<int> const pair_sums{3, 5, 7, 9};

// triple sums of {1,2,3,4,5}
inline std::vector<int> const triple_sums{6, 9, 12};

inline auto const not_three = [](int i)
{
    return i != 3;
};

inline auto const less_than_four = [](int i)
{
    return i < 4;
};

// {1,2,3,4,5} without the 3 -> {1,2,4,5}, then pairwise sums
inline std::vector<int> const filtered_pair_sums{3, 6, 9};

// concept checks use radr::test::check_adaptor_concepts (tests/include/radr/test/adaptor_template.hpp);
// only the concepts expected to be *true* are listed, anything not listed is expected to be false.

// --------------------------------------------------------------------------
// single-pass tests
// --------------------------------------------------------------------------

// radr::adjacent_transform cannot be created on single-pass ranges

// --------------------------------------------------------------------------
// multi-pass tests I – canonical cases
// --------------------------------------------------------------------------

TEST(adjacent_transform_mp, forward)
{
    auto ra = std::ref(flst) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, pair_sums);
    // std::forward_list is forward-only and not sized; common + borrowed are preserved.
    // add() returns a prvalue -> neither mutable nor (because the iterators are not const-symmetric) constant.
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .common = true, .borrowed = true});
}

TEST(adjacent_transform_mp, forward_rvalue)
{
    auto ra = radr::detail::decay_copy(flst) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, pair_sums);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .common = true});
}

TEST(adjacent_transform_mp, bidi_common)
{
    auto ra = std::ref(lst) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, pair_sums);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .borrowed = true});
}

TEST(adjacent_transform_mp, bidi_common_rvalue)
{
    auto ra = radr::detail::decay_copy(lst) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, pair_sums);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .sized = true, .common = true});
}

TEST(adjacent_transform_mp, ra_sized)
{
    auto ra = std::ref(deq) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, pair_sums);
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(adjacent_transform_mp, ra_sized_rvalue)
{
    auto ra = radr::detail::decay_copy(deq) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, pair_sums);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .sized = true, .common = true});
}

TEST(adjacent_transform_mp, contig_sized)
{
    auto ra = std::ref(vec) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, pair_sums);
    // contiguous input, but add() returns a prvalue -> .cat is ra, not contig
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(adjacent_transform_mp, contig_sized_rvalue)
{
    auto ra = radr::detail::decay_copy(vec) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, pair_sums);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .sized = true, .common = true});
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

TEST(adjacent_transform_mp, mutate)
{
    std::vector<int> v{1, 2, 3, 4};
    auto             ra = std::ref(v) | radr::adjacent_transform<2>(first_ref);

    for (int & i : ra)
        i += 10;

    // every element but the last is the 0th element of some window
    EXPECT_RANGE_EQ(v, (std::vector<int>{11, 12, 13, 4}));
}

TEST(adjacent_transform_mp, empty)
{
    std::vector<int> v{};
    auto             ra = std::ref(v) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{}));
    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(adjacent_transform_mp, constant)
{
    auto ra = std::cref(vec) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, pair_sums);
    // std::cref -> the iterators are const-symmetric and the prvalue reference is constant
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(adjacent_transform_mp, infinite)
{
    auto ra = radr::iota(0) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra | radr::take(3), (std::vector<int>{1, 3, 5}));
    // unbounded: neither sized nor common; radr::iota is constant
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .infinite = true, .constant = true, .borrowed = true});
}

TEST(adjacent_transform_mp, fwd_uncommon)
{
    auto ra = std::ref(flst) | radr::filter(not_three) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, filtered_pair_sums);
    // radr::filter is neither common nor sized, but it is a constant range, so the iterators stay const-symmetric
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
}

TEST(adjacent_transform_mp, bidi_uncommon)
{
    auto ra = std::ref(vec) | radr::filter(not_three) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, filtered_pair_sums);
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .constant = true, .borrowed = true});
}

TEST(adjacent_transform_mp, ra_nonsized)
{
    auto ra = std::ref(vec) | radr::take_while(less_than_four) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{3, 5}));
    // radr::take_while is random-access, but neither sized nor common
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .borrowed = true});
}

TEST(adjacent_transform_mp, contig_nonsized)
{
    auto ra = std::ref(deq) | radr::take_while(less_than_four) | radr::adjacent_transform<2>(add);

    EXPECT_RANGE_EQ(ra, (std::vector<int>{3, 5}));
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

TEST(adjacent_transform_mp, N1)
{
    // N == 1 invokes the functor with a single argument
    auto ra = std::ref(vec) | radr::adjacent_transform<1>([](int i) { return i * 2; });

    EXPECT_RANGE_EQ(ra, (std::vector<int>{2, 4, 6, 8, 10}));
    EXPECT_EQ(std::ranges::size(ra), 5u);
}

TEST(adjacent_transform_mp, N3)
{
    auto ra = std::ref(vec) | radr::adjacent_transform<3>(add3);

    EXPECT_RANGE_EQ(ra, triple_sums);
    EXPECT_EQ(std::ranges::size(ra), 3u);
}

TEST(adjacent_transform_mp, N_equals_size)
{
    auto ra =
      std::ref(vec) | radr::adjacent_transform<5>([](int a, int b, int c, int d, int e) { return a + b + c + d + e; });

    EXPECT_EQ(std::ranges::size(ra), 1u);
    EXPECT_RANGE_EQ(ra, (std::vector<int>{15}));
}

TEST(adjacent_transform_mp, N_larger_than_size)
{
    auto ra = std::ref(vec) | radr::adjacent_transform<6>([](int a, int b, int c, int d, int e, int f)
    { return a + b + c + d + e + f; });

    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(std::ranges::empty(ra));
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(adjacent_transform_mp, N_larger_than_size_forward)
{
    // non-random-access path: emptiness is detected via the last array element
    auto ra = std::ref(flst) | radr::adjacent_transform<9>([](auto... is) { return (is + ...); });

    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(adjacent_transform_mp, call_patterns)
{
    // the functor is not a range, so all three canonical call patterns are unambiguous
    auto piped   = std::ref(vec) | radr::adjacent_transform<2>(add);
    auto direct  = radr::adjacent_transform<2>(std::ref(vec), add);
    auto closure = radr::adjacent_transform<2>(add)(std::ref(vec));

    EXPECT_SAME_TYPE(decltype(piped), decltype(direct));
    EXPECT_SAME_TYPE(decltype(piped), decltype(closure));
    EXPECT_RANGE_EQ(piped, pair_sums);
    EXPECT_RANGE_EQ(direct, pair_sums);
    EXPECT_RANGE_EQ(closure, pair_sums);
}

TEST(adjacent_transform_mp, no_intermediate_tuple)
{
    auto ra = std::ref(vec) | radr::adjacent_transform<2>(add);

    // the functor is invoked N-ary; no std::tuple appears in any associated type
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, int);
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(ra)>, int);
    EXPECT_SAME_TYPE(radr::detail::range_const_reference_t<decltype(ra)>, int);
}

TEST(adjacent_transform_mp, reference_returning_functor)
{
    auto ra = std::ref(vec) | radr::adjacent_transform<2>(first_ref);

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, int &);
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(ra)>, int);
    // a reference-returning functor keeps mutability
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(adjacent_transform_mp, windows_overlap)
{
    auto ra = std::ref(vec) | radr::adjacent_transform<2>([](int a, int b) { return b - a; });

    // consecutive windows share an element
    EXPECT_RANGE_EQ(ra, (std::vector<int>{1, 1, 1, 1}));
}

TEST(adjacent_transform_mp, string_view_is_borrowed_and_constant)
{
    std::string_view sv{"abcd"};
    auto             ra = sv | radr::adjacent_transform<2>([](char a, char b) { return int(b - a); });

    EXPECT_RANGE_EQ(ra, (std::vector<int>{1, 1, 1}));
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(adjacent_transform_mp, iterator_operations)
{
    auto ra = std::ref(vec) | radr::adjacent_transform<2>(add);
    auto it = ra.begin();

    EXPECT_EQ(*it, 3);
    ++it;
    EXPECT_EQ(*it, 5);
    --it;
    EXPECT_EQ(*it, 3);

    EXPECT_EQ(it[0], 3);
    EXPECT_EQ(it[3], 9);

    it += 2;
    EXPECT_EQ(*it, 7);
    it -= 1;
    EXPECT_EQ(*it, 5);

    EXPECT_EQ(ra.end() - ra.begin(), 4);
    EXPECT_TRUE(ra.begin() < ra.end());
}

TEST(adjacent_transform_mp, nested)
{
    auto ra = std::ref(vec) | radr::adjacent_transform<2>(add) | radr::adjacent_transform<2>(add);

    // {3,5,7,9} -> {8,12,16}
    EXPECT_RANGE_EQ(ra, (std::vector<int>{8, 12, 16}));
}

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(adjacent_transform_mp, deep_copy)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::adjacent_transform<2>(add));

    T cpy;

    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::adjacent_transform<2>(add);
        EXPECT_RANGE_EQ(own, pair_sums);
        cpy = own;
    }

    EXPECT_RANGE_EQ(cpy, pair_sums);
}
