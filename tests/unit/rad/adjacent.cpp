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
#    include <list>
#    include <ranges>
#    include <string>
#    include <string_view>
#    include <tuple>
#    include <vector>

#    include <radr/test/aux_ranges.hpp>
#    include <radr/test/gtest_helpers.hpp>

#    include <radr/factory/iota.hpp>
#    include <radr/rad/adjacent.hpp>
#    include <radr/rad/elements.hpp>
#    include <radr/rad/take.hpp>
#    include <radr/rad/to_single_pass.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

inline std::vector<int> ints{1, 2, 3, 4, 5};
inline std::list<int>   ints_list{1, 2, 3, 4, 5};

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

// strings make the "same element appears in multiple tuples" aspect obvious
inline std::vector<std::string> strs{"foo", "bar", "baz"};

inline std::vector<std::tuple<std::string, std::string>> const str_pairs{
  {"foo", "bar"},
  {"bar", "baz"}
};

//!\brief Whether radr::adjacent<2> can be applied to URange at all.
template <typename URange>
concept adjacentable = requires(URange && urange) { std::forward<URange>(urange) | radr::adjacent<2>; };

// --------------------------------------------------------------------------
// single-pass: adjacent must not be available at all
// --------------------------------------------------------------------------

TEST(adjacent_sp, not_available)
{
    // multi-pass ranges are fine
    EXPECT_TRUE((adjacentable<std::vector<int>>));
    EXPECT_TRUE((adjacentable<std::reference_wrapper<std::vector<int>>>));
    EXPECT_TRUE((adjacentable<std::string_view>));

    // single-pass ranges are not
    EXPECT_FALSE((adjacentable<radr::generator<size_t>>));
    EXPECT_FALSE((adjacentable<decltype(std::vector<int>{} | radr::to_single_pass)>));
}

// --------------------------------------------------------------------------
// multi-pass: basic sliding-window semantics
// --------------------------------------------------------------------------

TEST(adjacent_mp, N2)
{
    auto ra = std::ref(ints) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    EXPECT_EQ(std::ranges::size(ra), 4u);
}

TEST(adjacent_mp, N3)
{
    auto ra = std::ref(ints) | radr::adjacent<3>;

    EXPECT_RANGE_EQ(ra, triples);
    EXPECT_EQ(std::ranges::size(ra), 3u);
}

TEST(adjacent_mp, N1)
{
    // N == 1 is legal and yields 1-tuples of every element
    auto ra = std::ref(ints) | radr::adjacent<1>;

    std::vector<std::tuple<int>> const comp{{1}, {2}, {3}, {4}, {5}};

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_EQ(std::ranges::size(ra), 5u);
}

TEST(adjacent_mp, N_equals_size)
{
    auto ra = std::ref(ints) | radr::adjacent<5>;

    EXPECT_EQ(std::ranges::size(ra), 1u);

    auto it = ra.begin();
    ASSERT_TRUE(it != ra.end());
    EXPECT_EQ(std::get<0>(*it), 1);
    EXPECT_EQ(std::get<4>(*it), 5);
    EXPECT_TRUE(++it == ra.end());
}

TEST(adjacent_mp, N_larger_than_size)
{
    auto ra = std::ref(ints) | radr::adjacent<6>;

    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(ra.begin() == ra.end());
    EXPECT_TRUE(std::ranges::empty(ra));
}

TEST(adjacent_mp, N_much_larger_than_size)
{
    auto ra = std::ref(ints) | radr::adjacent<100>;

    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(adjacent_mp, empty_range)
{
    std::vector<int> v{};
    auto             ra = std::ref(v) | radr::adjacent<2>;

    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(adjacent_mp, strings)
{
    auto ra = std::ref(strs) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, str_pairs);
}

TEST(adjacent_mp, rvalue_container)
{
    // rvalue → owning_rad
    auto ra = std::vector<int>{1, 2, 3, 4, 5} | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    EXPECT_FALSE((std::ranges::borrowed_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::random_access_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::common_range<decltype(ra)>));
}

// --------------------------------------------------------------------------
// value and reference types
// --------------------------------------------------------------------------

TEST(adjacent_mp, value_and_reference_types)
{
    auto ra = std::ref(ints) | radr::adjacent<2>;

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, (std::tuple<int &, int &>));
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(ra)>, (std::tuple<int, int>));
    EXPECT_SAME_TYPE(radr::detail::range_const_reference_t<decltype(ra)>, (std::tuple<int const &, int const &>));
}

TEST(adjacent_mp, value_and_reference_types_N3)
{
    auto ra = std::ref(strs) | radr::adjacent<3>;

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>,
                     (std::tuple<std::string &, std::string &, std::string &>));
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(ra)>, (std::tuple<std::string, std::string, std::string>));
}

TEST(adjacent_mp, const_range_yields_const_refs)
{
    auto const ra = std::ref(ints) | radr::adjacent<2>;

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, (std::tuple<int const &, int const &>));
}

// --------------------------------------------------------------------------
// concept preservation
// --------------------------------------------------------------------------

TEST(adjacent_mp, concepts_vector_ra_sized_common)
{
    auto ra = std::ref(ints) | radr::adjacent<2>;

    EXPECT_TRUE((std::ranges::random_access_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::common_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));
    EXPECT_TRUE((radr::mutable_range<decltype(ra)>));
    EXPECT_FALSE((radr::constant_range<decltype(ra)>));
}

TEST(adjacent_mp, concepts_deque_ra_sized_common)
{
    std::deque<int> d{1, 2, 3, 4, 5};
    auto            ra = std::ref(d) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    EXPECT_TRUE((std::ranges::random_access_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::common_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));
}

TEST(adjacent_mp, concepts_list_bidi_sized_common)
{
    // list: bidi + sized + common → categories capped at bidi, common preserved
    std::list<int> l{1, 2, 3, 4, 5};
    auto           ra = std::ref(l) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    EXPECT_TRUE((std::ranges::bidirectional_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::random_access_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::common_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));
}

TEST(adjacent_mp, concepts_forward_list_fwd_unsized)
{
    // forward_list: forward + common, but not sized and not bidi
    // → the end cannot be computed cheaply, so common_range is lost
    std::forward_list<int> fl{1, 2, 3, 4, 5};
    auto                   ra = std::ref(fl) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    EXPECT_TRUE((std::ranges::forward_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::bidirectional_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::common_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));
}

TEST(adjacent_mp, forward_list_shorter_than_N)
{
    // the "all other cases" branch has to detect emptiness via the sentinel
    std::forward_list<int> fl{1, 2};
    auto                   ra = std::ref(fl) | radr::adjacent<3>;

    EXPECT_TRUE(ra.begin() == ra.end());
}

TEST(adjacent_mp, concepts_string_view_constant)
{
    // string_view is borrowed already and constant
    std::string_view sv{"abcd"};
    auto             ra = sv | radr::adjacent<2>;

    EXPECT_TRUE((std::ranges::random_access_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::common_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));
    EXPECT_TRUE((radr::constant_range<decltype(ra)>));

    std::vector<std::tuple<char, char>> const comp{
      {'a', 'b'},
      {'b', 'c'},
      {'c', 'd'}
    };
    EXPECT_RANGE_EQ(ra, comp);
}

TEST(adjacent_mp, concepts_infinite)
{
    auto ra = radr::iota(0) | radr::adjacent<2>;

    EXPECT_TRUE((radr::infinite_mp_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::common_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));

    std::vector<std::tuple<int, int>> const comp{
      {0, 1},
      {1, 2},
      {2, 3}
    };
    EXPECT_RANGE_EQ(ra | radr::take(3), comp);
}

// --------------------------------------------------------------------------
// iterator operations
// --------------------------------------------------------------------------

TEST(adjacent_mp, iterator_increment_decrement)
{
    auto ra = std::ref(ints) | radr::adjacent<2>;
    auto it = ra.begin();

    EXPECT_EQ(*it, (std::tuple<int, int>{1, 2}));
    ++it;
    EXPECT_EQ(*it, (std::tuple<int, int>{2, 3}));
    ++it;
    EXPECT_EQ(*it, (std::tuple<int, int>{3, 4}));
    --it;
    EXPECT_EQ(*it, (std::tuple<int, int>{2, 3}));
}

TEST(adjacent_mp, iterator_random_access)
{
    auto ra = std::ref(ints) | radr::adjacent<2>;
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

TEST(adjacent_mp, sentinel_difference_forward_list)
{
    // no sized_sentinel_for → only equality comparison, but iteration must terminate
    std::forward_list<int> fl{1, 2, 3, 4, 5};
    auto                   ra = std::ref(fl) | radr::adjacent<2>;

    size_t count = 0;
    for ([[maybe_unused]] auto && t : ra)
        ++count;

    EXPECT_EQ(count, 4u);
}

// --------------------------------------------------------------------------
// mutation
// --------------------------------------------------------------------------

TEST(adjacent_mp, mutation_through_adjacent)
{
    std::vector<int> v{1, 2, 3, 4};
    auto             ra = std::ref(v) | radr::adjacent<2>;

    for (auto && [a, b] : ra)
        a += 10;

    // every element but the last one is the first element of some window
    EXPECT_EQ(v[0], 11);
    EXPECT_EQ(v[1], 12);
    EXPECT_EQ(v[2], 13);
    EXPECT_EQ(v[3], 4);
}

TEST(adjacent_mp, windows_alias_the_same_elements)
{
    std::vector<int> v{1, 2, 3};
    auto             ra = std::ref(v) | radr::adjacent<2>;

    auto it          = ra.begin();
    std::get<1>(*it) = 42; // this is v[1]

    ++it;
    EXPECT_EQ(std::get<0>(*it), 42); // ... and also the 0th element of the next window
    EXPECT_EQ(v[1], 42);
}

TEST(adjacent_mp, const_container_is_constant_range)
{
    std::vector<int> const v{1, 2, 3, 4, 5};
    auto                   ra = std::ref(v) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(ra, pairs);
    EXPECT_TRUE((radr::constant_range<decltype(ra)>));
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, (std::tuple<int const &, int const &>));
}

// --------------------------------------------------------------------------
// combination with other adaptors
// --------------------------------------------------------------------------

TEST(adjacent_mp, combine_with_elements)
{
    auto first  = std::ref(ints) | radr::adjacent<2> | radr::elements<0>;
    auto second = std::ref(ints) | radr::adjacent<2> | radr::elements<1>;

    EXPECT_RANGE_EQ(first, (std::vector<int>{1, 2, 3, 4}));
    EXPECT_RANGE_EQ(second, (std::vector<int>{2, 3, 4, 5}));
}

TEST(adjacent_mp, combine_with_elements_owning)
{
    auto first  = auto(ints) | radr::adjacent<2> | radr::elements<0>;
    auto second = auto(ints) | radr::adjacent<2> | radr::elements<1>;

    EXPECT_RANGE_EQ(first, (std::vector<int>{1, 2, 3, 4}));
    EXPECT_RANGE_EQ(second, (std::vector<int>{2, 3, 4, 5}));
}

TEST(adjacent_mp, combine_with_take_pre)
{
    auto first = std::ref(ints_list) | radr::take(5) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(first, pairs);
}

TEST(adjacent_mp, combine_with_take_pre_owning)
{
    auto first = auto(ints_list) | radr::take(5) | radr::adjacent<2>;

    EXPECT_RANGE_EQ(first, pairs);
}

TEST(adjacent_mp, combine_with_take)
{
    auto ra = std::ref(ints) | radr::adjacent<2> | radr::take(2);

    EXPECT_RANGE_EQ(ra,
                    (std::vector<std::tuple<int, int>>{
                      {1, 2},
                      {2, 3}
    }));
}

TEST(adjacent_mp, adjacent_of_adjacent)
{
    // windows of windows; the outer tuples contain the inner ones
    auto ra = std::ref(ints) | radr::adjacent<2> | radr::adjacent<2>;

    EXPECT_EQ(std::ranges::size(ra), 3u);

    auto it = ra.begin();
    EXPECT_EQ(std::get<0>(std::get<0>(*it)), 1);
    EXPECT_EQ(std::get<1>(std::get<0>(*it)), 2);
    EXPECT_EQ(std::get<0>(std::get<1>(*it)), 2);
    EXPECT_EQ(std::get<1>(std::get<1>(*it)), 3);
}

// --------------------------------------------------------------------------
// copyability
// --------------------------------------------------------------------------

TEST(adjacent_mp, owning_rad_is_copyable)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::adjacent<2>);
    T cpy;
    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::adjacent<2>;
        EXPECT_RANGE_EQ(own, pairs);
        cpy = own;
    }
    EXPECT_RANGE_EQ(cpy, pairs);
}

// copying an owning_rad rebinds only the first of the N underlying iterators and re-derives the others from it,
// so the cases below pin down the situations in which those re-derived iterators could go stale

TEST(adjacent_mp, owning_rad_is_copyable_N3)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::adjacent<3>);
    T cpy;
    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::adjacent<3>;
        cpy   = own;
    }
    EXPECT_RANGE_EQ(cpy, triples);
}

TEST(adjacent_mp, owning_rad_is_copyable_bidi)
{
    // std::list is not random access, so the underlying iterators are rebound/advanced step-wise
    using T = decltype(std::list<int>{1, 2, 3, 4, 5} | radr::adjacent<3>);
    T cpy;
    {
        T own = std::list<int>{1, 2, 3, 4, 5} | radr::adjacent<3>;
        cpy   = own;
    }
    EXPECT_RANGE_EQ(cpy, triples);
}

TEST(adjacent_mp, owning_rad_is_copyable_shorter_than_N)
{
    // the range is shorter than N, i.e. the trailing iterators collapse onto the end (gaps of 0)
    using T = decltype(std::vector<int>{1, 2} | radr::adjacent<4>);
    T cpy;
    {
        T own = std::vector<int>{1, 2} | radr::adjacent<4>;
        cpy   = own;
    }
    EXPECT_TRUE(std::ranges::empty(cpy));
    EXPECT_EQ(cpy.begin(), cpy.end());
}

TEST(adjacent_mp, owning_rad_is_copyable_shorter_than_N_bidi)
{
    // the range is shorter than N, i.e. the trailing iterators collapse onto the end (gaps of 0)
    using T = decltype(std::list<int>{1, 2, 3, 4, 5} | radr::take(3) | radr::adjacent<4>);
    T cpy;
    {
        T own = std::list<int>{1, 2, 3, 4, 5} | radr::take(3) | radr::adjacent<4>;
        cpy   = own;
    }
    EXPECT_TRUE(std::ranges::empty(cpy));
    EXPECT_EQ(cpy.begin(), cpy.end());
}

TEST(adjacent_mp, borrowing_rad_is_copyable)
{
    auto ra1 = std::ref(ints) | radr::adjacent<2>;
    auto ra2 = ra1;

    EXPECT_RANGE_EQ(ra1, pairs);
    EXPECT_RANGE_EQ(ra2, pairs);
}

#endif
