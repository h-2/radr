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
#    include <tuple>
#    include <vector>

#    include <radr/test/aux_ranges.hpp>
#    include <radr/test/gtest_helpers.hpp>

#    include <radr/rad/elements.hpp>
#    include <radr/rad/enumerate.hpp>
#    include <radr/rad/to_single_pass.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// strings make the index vs. value distinction obvious in assertions
inline std::vector<std::string>                              strs{"foo", "bar", "baz"};
inline std::vector<std::tuple<ptrdiff_t, std::string>> const comp{
  {0, "foo"},
  {1, "bar"},
  {2, "baz"}
};

// --------------------------------------------------------------------------
// single-pass tests
// --------------------------------------------------------------------------

TEST(enumerate_sp, basic)
{
    auto ra = radr::test::iota_input_range(10, 13) | radr::enumerate;

    std::vector<std::tuple<ptrdiff_t, size_t>> result;
    for (auto && [i, v] : ra)
        result.emplace_back(i, v);

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(std::get<0>(result[0]), 0);
    EXPECT_EQ(std::get<1>(result[0]), 10u);
    EXPECT_EQ(std::get<0>(result[1]), 1);
    EXPECT_EQ(std::get<1>(result[1]), 11u);
    EXPECT_EQ(std::get<0>(result[2]), 2);
    EXPECT_EQ(std::get<1>(result[2]), 12u);
}

TEST(enumerate_sp, value_and_reference_types)
{
    // iota_input_range yields size_t by value (generator<size_t>), so ref is size_t &&
    auto ra = radr::test::iota_input_range(0, 1) | radr::enumerate;

#    ifdef __cpp_lib_generator
    // with std::generator: ref_t is tuple<diff_t, size_t &&>, val_t is tuple<diff_t, size_t>
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, (std::tuple<ptrdiff_t &&, size_t &&> &&));
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(ra)>, (std::tuple<ptrdiff_t, size_t>));
#    else
    // without std::generator polyfill: ref_t collapses to value
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, (std::tuple<ptrdiff_t, size_t>));
#    endif
}

TEST(enumerate_sp, concept_checks)
{
    auto ra = radr::test::iota_input_range(0, 3) | radr::enumerate;

    EXPECT_TRUE((std::ranges::input_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::forward_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::common_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::borrowed_range<decltype(ra)>));
}

TEST(enumerate_sp, from_multipass_via_to_single_pass)
{
    // A multi-pass range demoted to single-pass should also go through enumerate_coro
    auto ra = std::vector<std::string>{"foo", "bar", "baz"} | radr::to_single_pass | radr::enumerate;

    std::vector<std::tuple<ptrdiff_t, std::string>> result;
    for (auto && [i, v] : ra)
        result.emplace_back(i, v);

    EXPECT_RANGE_EQ(result, comp);
}

// --------------------------------------------------------------------------
// multi-pass: sized + common (e.g. vector, deque, string_view)
// -> special case: bounded iota, common zip_iterator endpoint
// --------------------------------------------------------------------------

TEST(enumerate_mp, vector_rvalue_sized_common)
{
    // rvalue → owning_rad
    auto ra = std::vector<std::string>{"foo", "bar", "baz"} | radr::enumerate;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_FALSE((std::ranges::borrowed_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::random_access_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::common_range<decltype(ra)>));
}

TEST(enumerate_mp, vector_lvalue_sized_common)
{
    // lvalue → borrowing_rad; vector is RA+sized+common
    auto ra = std::ref(strs) | radr::enumerate;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::random_access_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::common_range<decltype(ra)>));
}

TEST(enumerate_mp, vector_lvalue_value_and_reference_types)
{
    auto ra = std::ref(strs) | radr::enumerate;

    // mutable: index is ptrdiff_t (by value from iota), value is lvalue ref
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, (std::tuple<ptrdiff_t, std::string &>));
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(ra)>, (std::tuple<ptrdiff_t, std::string>));

    // const iteration: index is ptrdiff_t, value is const lvalue ref
    EXPECT_SAME_TYPE(radr::detail::range_const_reference_t<decltype(ra)>, (std::tuple<ptrdiff_t, std::string const &>));
}

TEST(enumerate_mp, deque_lvalue_sized_common)
{
    // deque: RA+sized+common — same special-case branch as vector
    std::deque<std::string> d{"foo", "bar", "baz"};
    auto                    ra = std::ref(d) | radr::enumerate;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::random_access_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::common_range<decltype(ra)>));
}

TEST(enumerate_mp, string_view_sized_common)
{
    // string_view is already a borrowed range (no std::ref needed)
    std::string_view sv{"abc"};
    auto             ra = sv | radr::enumerate;

    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::random_access_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::sized_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::common_range<decltype(ra)>));

    auto it = ra.begin();
    EXPECT_EQ(std::get<0>(*it), ptrdiff_t{0});
    EXPECT_EQ(std::get<1>(*it), 'a');
    ++it;
    EXPECT_EQ(std::get<0>(*it), ptrdiff_t{1});
    EXPECT_EQ(std::get<1>(*it), 'b');
}

// --------------------------------------------------------------------------
// multi-pass: sized + !common (e.g. list — bidi+sized, but bidi zip is not common)
// -> zip_with_borrow with bounded iota, zip_sentinel endpoint, has size
// --------------------------------------------------------------------------

TEST(enumerate_mp, list_lvalue_sized_not_common)
{
    std::list<std::string> l{"foo", "bar", "baz"};
    auto                   ra = std::ref(l) | radr::enumerate;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::bidirectional_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::random_access_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::sized_range<decltype(ra)>));
    // bidi zip loses common_range (zip_sentinel has different type than zip_iterator)
    // but enumerate preserves it
    EXPECT_TRUE((std::ranges::common_range<decltype(ra)>));
}

// --------------------------------------------------------------------------
// multi-pass: !sized + common (e.g. forward_list — forward+common, no size)
// -> zip_with_borrow with unbounded iota, zip_sentinel endpoint, no size
// --------------------------------------------------------------------------

TEST(enumerate_mp, forward_list_lvalue_unsized_not_common)
{
    std::forward_list<std::string> fl{"foo", "bar", "baz"};
    auto                           ra = std::ref(fl) | radr::enumerate;

    EXPECT_RANGE_EQ(ra, comp);
    EXPECT_TRUE((std::ranges::borrowed_range<decltype(ra)>));
    EXPECT_TRUE((std::ranges::forward_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::bidirectional_range<decltype(ra)>));
    EXPECT_FALSE((std::ranges::sized_range<decltype(ra)>));
    // forward_list is common, but iota is infinite → zip falls to "other cases"
    // → zip_sentinel endpoint → not common
    // enumerate cannot work around this, because forward_list is not sized
    EXPECT_FALSE((std::ranges::common_range<decltype(ra)>));
}

// --------------------------------------------------------------------------
// index type matches range_difference_t of the underlying range
// --------------------------------------------------------------------------

TEST(enumerate_mp, index_type_is_range_difference_t)
{
    auto ra = std::ref(strs) | radr::enumerate;
    auto it = ra.begin();

    using diff_t = std::ranges::range_difference_t<std::vector<std::string>>;
    EXPECT_SAME_TYPE(decltype(std::get<0>(*it)), diff_t &&);
}

// --------------------------------------------------------------------------
// const-correctness: iterating over a const enumerate adaptor yields const refs
// --------------------------------------------------------------------------

TEST(enumerate_mp, const_range_yields_const_refs)
{
    auto const ra = std::ref(strs) | radr::enumerate;

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(ra)>, (std::tuple<ptrdiff_t, std::string const &>));
}

// --------------------------------------------------------------------------
// mutation through enumerate
// --------------------------------------------------------------------------

TEST(enumerate_mp, mutation_through_enumerate)
{
    std::vector<std::string> v{"foo", "bar", "baz"};
    auto                     ra = std::ref(v) | radr::enumerate;

    for (auto [i, s] : ra)
        s += "!";

    EXPECT_EQ(v[0], "foo!");
    EXPECT_EQ(v[1], "bar!");
    EXPECT_EQ(v[2], "baz!");
}

// --------------------------------------------------------------------------
// empty range
// --------------------------------------------------------------------------

TEST(enumerate_mp, empty_range)
{
    std::vector<std::string> v{};
    auto                     ra = std::ref(v) | radr::enumerate;

    EXPECT_EQ(std::ranges::size(ra), 0u);
    EXPECT_TRUE(ra.begin() == ra.end());
}

// --------------------------------------------------------------------------
// combine with elements
// --------------------------------------------------------------------------

TEST(enumerate_mp, combine_with_elements)
{
    std::vector<std::string> v  = strs;
    auto                     ra = std::ref(v) | radr::enumerate | radr::elements<1>;

    EXPECT_RANGE_EQ(v, ra);
    EXPECT_RANGE_EQ(strs, ra);
}

TEST(enumerate_mp, combine_with_elements_rvalue)
{
    std::vector<std::string> v  = strs;
    auto                     ra = std::move(v) | radr::enumerate | radr::elements<1>;

    EXPECT_TRUE(v.empty());
    EXPECT_RANGE_EQ(strs, ra);
}

// --------------------------------------------------------------------------
// copyability
// --------------------------------------------------------------------------

TEST(enumerate_mp, owning_rad_is_copyable)
{
    using T = decltype(std::vector<std::string>{"foo", "bar", "baz"} | radr::enumerate);
    T cpy;
    {
        T own = std::vector<std::string>{"foo", "bar", "baz"} | radr::enumerate;
        EXPECT_RANGE_EQ(own, comp);
        cpy = own;
    }
    EXPECT_RANGE_EQ(cpy, comp);
}

TEST(enumerate_mp, borrowing_rad_is_copyable)
{
    auto ra1 = std::ref(strs) | radr::enumerate;
    auto ra2 = ra1;

    EXPECT_RANGE_EQ(ra1, comp);
    EXPECT_RANGE_EQ(ra2, comp);
}

#endif
