// Structured re-implementation of the radr::adjacent test-suite, following UNIT_TEST_TEMPLATE.cxx.

#include <gtest/gtest.h>

#include <radr/version.hpp>

#if !RADR_FEATURE_ZIP

TEST(dummy, skipped_because_no_cpp23)
{
    GTEST_SKIP() << "Requires C++23";
}

#else

#    include <deque>
#    include <forward_list>
#    include <functional>
#    include <list>
#    include <ranges>
#    include <string_view>
#    include <tuple>
#    include <vector>

#    include <radr/test/adaptor_template.hpp>
#    include <radr/test/aux_ranges.hpp>
#    include <radr/test/gtest_helpers.hpp>

#    include <radr/factory/iota.hpp>
#    include <radr/rad/adjacent.hpp>
#    include <radr/rad/elements.hpp>
#    include <radr/rad/filter.hpp>
#    include <radr/rad/take.hpp>
#    include <radr/rad/take_while.hpp>
#    include <radr/rad/to_single_pass.hpp>

using radr::test::range_cat;

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// non-const, but only ever read (mutation tests use their own locals)
inline std::vector<int>       vec{1, 2, 3, 4, 5};
inline std::list<int>         lst{1, 2, 3, 4, 5};
inline std::forward_list<int> flst{1, 2, 3, 4, 5};
inline std::deque<int>        deq{1, 2, 3, 4, 5};

inline std::vector<std::tuple<int, int>> const pairs{
  {1, 2},
  {2, 3},
  {3, 4},
  {4, 5}
};

inline std::vector<std::tuple<int, int, int>> const triples{
  {1, 2, 3},
  {2, 3, 4},
  {3, 4, 5}
};

// {1,2,3,4,5} with the 3 filtered out -> {1,2,4,5}, then adjacent<2>
inline auto const not_three = [](int i)
{
    return i != 3;
};
inline std::vector<std::tuple<int, int>> const filtered_pairs{
  {1, 2},
  {2, 4},
  {4, 5}
};

// concept checks use radr::test::check_adaptor_concepts (tests/include/radr/test/adaptor_template.hpp);
// only the concepts expected to be *true* are listed, anything not listed is expected to be false.

// --------------------------------------------------------------------------
// single-pass tests
// --------------------------------------------------------------------------

// radr::adjacent cannot be created on single-pass ranges

// --------------------------------------------------------------------------
// multi-pass tests I – canonical cases
// --------------------------------------------------------------------------

TEST(adjacent_mp, forward)
{
    auto ra = std::ref(flst) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    // std::forward_list is forward-only and not sized; common + borrowed are preserved
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::fwd, .common = true, .mut = true, .borrowed = true});
}

TEST(adjacent_mp, forward_rvalue)
{
    auto ra = auto{flst} | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .common = true, .mut = true});
}

TEST(adjacent_mp, bidi_common)
{
    auto ra = std::ref(lst) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    // std::list: sized + common preserved
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(adjacent_mp, bidi_common_rvalue)
{
    auto ra = auto{lst} | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::bidi, .sized = true, .common = true, .mut = true});
}

TEST(adjacent_mp, ra_sized)
{
    auto ra = std::ref(deq) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    // std::deque: sized + common preserved
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(adjacent_mp, ra_sized_rvalue)
{
    auto ra = auto{deq} | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true});
}

TEST(adjacent_mp, contig_sized)
{
    auto ra = std::ref(vec) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    // std::vector input is contiguous, but adjacent yields tuples -> .cat is ra, not contig
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(adjacent_mp, contig_sized_rvalue)
{
    auto ra = auto{vec} | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    // contiguous input, but adjacent yields tuples -> .cat is ra; rvalue input -> .borrowed == false
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true});
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

TEST(adjacent_mp, mutate)
{
    std::vector<int> v{1, 2, 3, 4};
    auto             ra = std::ref(v) | radr::adjacent<2>;

    for (auto && [a, b] : ra)
        a += 10;

    // every element but the last is the 0th element of some window
    EXPECT_RANGE_EQ(v, (std::vector<int>{11, 12, 13, 4}));
}

TEST(adjacent_mp, empty)
{
    std::vector<int> v{};
    auto             ra = std::ref(v) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, (std::vector<std::tuple<int, int>>{}));
    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(adjacent_mp, constant)
{
    auto ra = std::cref(vec) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    // std::cref -> constant range, hence .mut == false
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, (std::tuple<int const &, int const &>));
}

TEST(adjacent_mp, infinite)
{
    auto ra = radr::iota(0) | radr::adjacent<2>;

    std::vector<std::tuple<int, int>> const head{
      {0, 1},
      {1, 2},
      {2, 3}
    };
    EXPECT_RANGE_EQ(ra | radr::take(3), head);

    // unbounded: .sized == false and .common == false; radr::iota is a constant range, hence .mut == false
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .infinite = true, .constant = true, .borrowed = true});
}

TEST(adjacent_mp, fwd_uncommon)
{
    auto ra = std::ref(flst) | radr::filter(not_three) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, filtered_pairs);
    // std::forward_list | filter: .common == false (filter) and not sized;
    // radr::filter is a constant range, hence .mut == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::fwd, .constant = true, .borrowed = true});
}

TEST(adjacent_mp, bidi_uncommon)
{
    auto ra = std::ref(vec) | radr::filter(not_three) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, filtered_pairs);
    // std::vector | filter: bidi but .common == false (the bidi+common branch does not apply) and not sized;
    // radr::filter is a constant range, hence .mut == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::bidi, .constant = true, .borrowed = true});
}

TEST(adjacent_mp, ra_nonsized)
{
    auto ra = std::ref(vec) | radr::take_while([](int i) { return i < 4; }) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra,
                    (std::vector<std::tuple<int, int>>{
                      {1, 2},
                      {2, 3}
    }));
    // std::vector | take_while: random-access, but .sized == false and .common == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .mut = true, .borrowed = true});
}

TEST(adjacent_mp, contig_nonsized)
{
    auto ra = std::ref(deq) | radr::take_while([](int i) { return i < 4; }) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra,
                    (std::vector<std::tuple<int, int>>{
                      {1, 2},
                      {2, 3}
    }));
    // std::deque | take_while: random-access, but .sized == false and .common == false
    radr::test::check_adaptor_concepts<decltype(ra)>({.cat = range_cat::ra, .mut = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

TEST(adjacent_mp, N1)
{
    // N == 1 yields 1-tuples of every element
    auto ra = std::ref(vec) | radr::adjacent<1>;

    EXPECT_RANGE_EQ(ra, (std::vector<std::tuple<int>>{{1}, {2}, {3}, {4}, {5}}));
    EXPECT_EQ(std::ranges::size(ra), 5u);
}

TEST(adjacent_mp, N3)
{
    auto ra = std::ref(vec) | radr::adjacent<3>;

    EXPECT_RANGE_EQ(ra, triples);
    EXPECT_EQ(std::ranges::size(ra), 3u);
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, (std::tuple<int &, int &, int &>));
}

TEST(adjacent_mp, N_equals_size)
{
    auto ra = std::ref(vec) | radr::adjacent<5>;

    EXPECT_EQ(std::ranges::size(ra), 1u);

    auto it = ra.begin();
    ASSERT_TRUE(it != ra.end());
    EXPECT_EQ(std::get<0>(*it), 1);
    EXPECT_EQ(std::get<4>(*it), 5);
    EXPECT_TRUE(++it == ra.end());
}

TEST(adjacent_mp, N_larger_than_size)
{
    auto ra = std::ref(vec) | radr::adjacent<6>;

    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(std::ranges::empty(ra));
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(adjacent_mp, N_larger_than_size_forward)
{
    // non-random-access path: emptiness is detected via the last array element
    auto ra = std::ref(flst) | radr::adjacent<9>;

    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(adjacent_mp, windows_alias_the_same_elements)
{
    std::vector<int> v{1, 2, 3};
    auto             ra = std::ref(v) | radr::adjacent<2>;

    auto it          = ra.begin();
    std::get<1>(*it) = 42; // v[1]
    ++it;
    EXPECT_EQ(std::get<0>(*it), 42); // ...also the 0th element of the next window
    EXPECT_EQ(v[1], 42);
}

TEST(adjacent_mp, value_and_reference_types)
{
    auto ra = std::ref(vec) | radr::adjacent<2>;

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, (std::tuple<int &, int &>));
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(ra)>, (std::tuple<int, int>));
    EXPECT_SAME_TYPE(radr::detail::range_const_reference_t<decltype(ra)>, (std::tuple<int const &, int const &>));
}

TEST(adjacent_mp, iterator_increment_decrement)
{
    auto ra = std::ref(vec) | radr::adjacent<2>;
    auto it = ra.begin();

    EXPECT_EQ(*it, (std::tuple<int, int>{1, 2}));
    ++it;
    EXPECT_EQ(*it, (std::tuple<int, int>{2, 3}));
    --it;
    EXPECT_EQ(*it, (std::tuple<int, int>{1, 2}));
}

TEST(adjacent_mp, iterator_random_access)
{
    auto ra = std::ref(vec) | radr::adjacent<2>;
    auto it = ra.begin();

    EXPECT_EQ(it[0], (std::tuple<int, int>{1, 2}));
    EXPECT_EQ(it[3], (std::tuple<int, int>{4, 5}));

    it += 2;
    EXPECT_EQ(*it, (std::tuple<int, int>{3, 4}));
    it -= 1;
    EXPECT_EQ(*it, (std::tuple<int, int>{2, 3}));

    EXPECT_EQ(ra.end() - ra.begin(), 4);
    EXPECT_TRUE(ra.begin() < ra.end());
}

TEST(adjacent_mp, string_view_is_borrowed_and_constant)
{
    std::string_view sv{"abcd"};
    auto             ra = sv | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra,
                    (std::vector<std::tuple<char, char>>{
                      {'a', 'b'},
                      {'b', 'c'},
                      {'c', 'd'}
    }));
    // contiguous input, but adjacent yields tuples -> .cat is ra; string_view elements are const -> .mut == false
    radr::test::check_adaptor_concepts<decltype(ra)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(adjacent_mp, adjacent_of_adjacent)
{
    // windows of windows; outer tuples contain the inner ones
    auto ra = std::ref(vec) | radr::adjacent<2> | radr::adjacent<2>;

    EXPECT_EQ(std::ranges::size(ra), 3u);

    auto it = ra.begin();
    EXPECT_EQ(std::get<0>(std::get<0>(*it)), 1);
    EXPECT_EQ(std::get<1>(std::get<0>(*it)), 2);
    EXPECT_EQ(std::get<0>(std::get<1>(*it)), 2);
    EXPECT_EQ(std::get<1>(std::get<1>(*it)), 3);
}

TEST(adjacent_mp, combine_with_elements)
{
    auto first  = std::ref(vec) | radr::adjacent<2> | radr::elements<0>;
    auto second = std::ref(vec) | radr::adjacent<2> | radr::elements<1>;

    EXPECT_RANGE_EQ(first, (std::vector<int>{1, 2, 3, 4}));
    EXPECT_RANGE_EQ(second, (std::vector<int>{2, 3, 4, 5}));
}

TEST(adjacent_mp, combine_with_take)
{
    auto ra = std::ref(vec) | radr::adjacent<2> | radr::take(2);

    EXPECT_RANGE_EQ(ra,
                    (std::vector<std::tuple<int, int>>{
                      {1, 2},
                      {2, 3}
    }));
}

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(adjacent_mp, deep_copy)
{
    using T = decltype(auto{vec} | radr::adjacent<2>);
    T cpy;
    {
        T own = auto{vec} | radr::adjacent<2>;
        EXPECT_RANGE_EQ(own, pairs);
        cpy = own;
    }
    EXPECT_RANGE_EQ(cpy, pairs);
}

// copying an owning_rad rebinds only the first of the N underlying iterators and re-derives the
// rest from it, so the following pin down the situations where those could go stale

TEST(adjacent_mp, deep_copy_N3)
{
    using T = decltype(auto{vec} | radr::adjacent<3>);
    T cpy;
    {
        T own = auto{vec} | radr::adjacent<3>;
        cpy   = own;
    }
    EXPECT_RANGE_EQ(cpy, triples);
}

TEST(adjacent_mp, deep_copy_bidi)
{
    // std::list is not random access, so the underlying iterators advance step-wise on re-derive
    using T = decltype(auto{lst} | radr::adjacent<3>);
    T cpy;
    {
        T own = auto{lst} | radr::adjacent<3>;
        cpy   = own;
    }
    EXPECT_RANGE_EQ(cpy, triples);
}

TEST(adjacent_mp, deep_copy_shorter_than_N)
{
    // range shorter than N: the trailing iterators collapse onto the end (gaps of 0)
    using T = decltype(std::vector<int>{1, 2} | radr::adjacent<4>);
    T cpy;
    {
        T own = std::vector<int>{1, 2} | radr::adjacent<4>;
        cpy   = own;
    }
    EXPECT_TRUE(std::ranges::empty(cpy));
    EXPECT_EQ(cpy.begin(), cpy.end());
}

TEST(adjacent_mp, borrowing_rad_is_copyable)
{
    auto ra1 = std::ref(vec) | radr::adjacent<2>;
    auto ra2 = ra1;

    EXPECT_RANGE_EQ(ra1, pairs);
    EXPECT_RANGE_EQ(ra2, pairs);
}

#endif
