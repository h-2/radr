// Test-suite for radr::zip_with_transform, following UNIT_TEST_TEMPLATE.cxx.
// NOTE: deliberately NOT guarded by RADR_FEATURE_ZIP; radr::zip_with_transform is available in C++20.

#include <deque>
#include <forward_list>
#include <functional>
#include <list>
#include <ranges>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/iota.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/take.hpp>
#include <radr/rad/take_while.hpp>
#include <radr/rad/to_single_pass.hpp>
#include <radr/rad/zip_with_transform.hpp>

using radr::test::range_cat;

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// non-const, but only ever read (mutation tests use their own locals)
inline std::vector<int>       vec{1, 2, 3, 4, 5};
inline std::list<int>         lst{1, 2, 3, 4, 5};
inline std::forward_list<int> flst{1, 2, 3, 4, 5};
inline std::deque<int>        deq{1, 2, 3, 4, 5};

// the "other" range of every canonical case; contiguous, sized and common, so the
// resulting concepts are governed by the range under test
inline std::vector<int> other{10, 20, 30, 40, 50};

inline constexpr auto add = [](int a, int b)
{
    return a + b;
};

// returns a reference into the first range; generic so that it also accepts the const iterators
inline constexpr auto first_ref = [](auto & a, auto &) -> auto &
{
    return a;
};

inline std::vector<int> const sums{11, 22, 33, 44, 55};

inline auto const not_three = [](int i)
{
    return i != 3;
};

inline auto const less_than_four = [](int i)
{
    return i < 4;
};

// {1,2,3,4,5} without the 3 -> {1,2,4,5}, zipped with {10,20,30,40}
inline std::vector<int> const filtered_sums{11, 22, 34, 45};

// concept checks use radr::test::check_adaptor_concepts (tests/include/radr/test/adaptor_template.hpp);
// only the concepts expected to be *true* are listed, anything not listed is expected to be false.

// --------------------------------------------------------------------------
// single-pass tests
// --------------------------------------------------------------------------

TEST(zip_with_transform_sp, simple)
{
    auto z = radr::iota_sp(1, 6) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_SAME_TYPE(decltype(z), (radr::generator<int, int>));
    EXPECT_RANGE_EQ(z, sums);
}

TEST(zip_with_transform_sp, single_range)
{
    auto z = radr::iota_sp(1, 4) | radr::zip_with_transform([](int i) { return i * 2; });

    EXPECT_SAME_TYPE(decltype(z), (radr::generator<int, int>));
    EXPECT_RANGE_EQ(z, (std::vector<int>{2, 4, 6}));
}

TEST(zip_with_transform_sp, three_ranges)
{
    std::vector<int> const a{100, 200, 300, 400, 500};
    auto                   z = radr::iota_sp(1, 6) |
             radr::zip_with_transform([](int x, int y, int z_) { return x + y + z_; }, std::ref(other), std::cref(a));

    EXPECT_RANGE_EQ(z, (std::vector<int>{111, 222, 333, 444, 555}));
}

TEST(zip_with_transform_sp, stops_at_shortest)
{
    std::vector<int> const shrt{10, 20};
    auto                   z = radr::iota_sp(1, 100) | radr::zip_with_transform(add, std::cref(shrt));

    EXPECT_RANGE_EQ(z, (std::vector<int>{11, 22}));
}

TEST(zip_with_transform_sp, from_multi_pass_input)
{
    auto z = std::vector<int>{1, 2, 3, 4, 5} | radr::to_single_pass | radr::zip_with_transform(add, std::ref(other));

    EXPECT_SAME_TYPE(decltype(z), (radr::generator<int, int>));
    EXPECT_RANGE_EQ(z, sums);
}

TEST(zip_with_transform_sp, mutable_functor)
{
    // the single-pass adaptor only requires std::invocable, so observable state is allowed
    auto z =
      radr::iota_sp(1, 4) | radr::zip_with_transform([calls = int{}](int i) mutable { return i * 10 + ++calls; });

    EXPECT_RANGE_EQ(z, (std::vector<int>{11, 22, 33}));
}

TEST(zip_with_transform_sp, reference_returning_functor)
{
    std::vector<std::string> strs{"a", "b", "c"};
    auto                     z = radr::iota_sp(0, 3) |
             radr::zip_with_transform([&strs](int i, int) -> std::string & { return strs[i]; }, std::ref(other));

    EXPECT_SAME_TYPE(decltype(z), (radr::generator<std::string &, std::string>));
    EXPECT_RANGE_EQ(z, (std::vector<std::string>{"a", "b", "c"}));
}

// --------------------------------------------------------------------------
// multi-pass tests I – canonical cases
// --------------------------------------------------------------------------

TEST(zip_with_transform_mp, forward)
{
    auto z = std::ref(flst) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    // std::forward_list is not sized; common survives because not all inputs are bidirectional.
    // add() returns a prvalue -> neither mutable nor (because the iterators are not const-symmetric) constant.
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::fwd, .common = true, .borrowed = true});
}

TEST(zip_with_transform_mp, forward_rvalue)
{
    auto z = radr::detail::decay_copy(flst) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::fwd, .common = true});
}

TEST(zip_with_transform_mp, bidi_common)
{
    auto z = std::ref(lst) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    // both inputs are sized, but all inputs are bidirectional and std::list is not indexable -> not common
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::bidi, .sized = true, .borrowed = true});
}

TEST(zip_with_transform_mp, bidi_common_rvalue)
{
    auto z = radr::detail::decay_copy(lst) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::bidi, .sized = true});
}

TEST(zip_with_transform_mp, ra_sized)
{
    auto z = std::ref(deq) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    // all inputs indexable and sized -> sized + common
    radr::test::check_adaptor_concepts<decltype(z)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(zip_with_transform_mp, ra_sized_rvalue)
{
    auto z = radr::detail::decay_copy(deq) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::ra, .sized = true, .common = true});
}

TEST(zip_with_transform_mp, contig_sized)
{
    auto z = std::ref(vec) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    // contiguous input, but add() returns a prvalue -> .cat is ra, not contig
    radr::test::check_adaptor_concepts<decltype(z)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(zip_with_transform_mp, contig_sized_rvalue)
{
    auto z = radr::detail::decay_copy(vec) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    // rvalue input -> owning_rad, hence .borrowed == false
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::ra, .sized = true, .common = true});
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

TEST(zip_with_transform_mp, mutate)
{
    std::vector<int> v{1, 2, 3};
    std::vector<int> w{10, 20, 30};
    auto             z = std::ref(v) | radr::zip_with_transform(first_ref, std::ref(w));

    for (int & i : z)
        i *= 2;

    EXPECT_RANGE_EQ(v, (std::vector<int>{2, 4, 6}));
    EXPECT_RANGE_EQ(w, (std::vector<int>{10, 20, 30}));
}

TEST(zip_with_transform_mp, empty)
{
    std::vector<int> v{};
    auto             z = std::ref(v) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, (std::vector<int>{}));
    EXPECT_EQ(std::ranges::size(z), 0u);
    EXPECT_TRUE(z.begin() == z.end());
}

TEST(zip_with_transform_mp, constant)
{
    auto z = std::cref(vec) | radr::zip_with_transform(add, std::cref(other));

    EXPECT_RANGE_EQ(z, sums);
    // both inputs constant -> the iterators are const-symmetric and the prvalue reference is constant
    radr::test::check_adaptor_concepts<decltype(z)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .constant = true, .borrowed = true});
}

TEST(zip_with_transform_mp, infinite)
{
    auto z = radr::iota(1) | radr::zip_with_transform(add, radr::iota(10));

    EXPECT_RANGE_EQ(z | radr::take(3), (std::vector<int>{11, 13, 15}));
    // unbounded: neither sized nor common; radr::iota is constant
    radr::test::check_adaptor_concepts<decltype(z)>(
      {.cat = range_cat::ra, .infinite = true, .constant = true, .borrowed = true});
}

TEST(zip_with_transform_mp, fwd_uncommon)
{
    auto z = std::ref(flst) | radr::filter(not_three) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, filtered_sums);
    // radr::filter is neither common nor sized
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::fwd, .borrowed = true});
}

TEST(zip_with_transform_mp, bidi_uncommon)
{
    auto z = std::ref(vec) | radr::filter(not_three) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, filtered_sums);
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::bidi, .borrowed = true});
}

TEST(zip_with_transform_mp, ra_nonsized)
{
    auto z = std::ref(vec) | radr::take_while(less_than_four) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, (std::vector<int>{11, 22, 33}));
    // radr::take_while is random-access, but neither sized nor common
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::ra, .borrowed = true});
}

TEST(zip_with_transform_mp, contig_nonsized)
{
    auto z = std::ref(deq) | radr::take_while(less_than_four) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, (std::vector<int>{11, 22, 33}));
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::ra, .borrowed = true});
}

// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

TEST(zip_with_transform_mp, single_range)
{
    auto z = std::ref(vec) | radr::zip_with_transform([](int i) { return i * 2; });

    EXPECT_RANGE_EQ(z, (std::vector<int>{2, 4, 6, 8, 10}));
    radr::test::check_adaptor_concepts<decltype(z)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(zip_with_transform_mp, three_ranges)
{
    std::vector<int> a{100, 200, 300, 400, 500};
    auto             z = std::ref(vec) |
             radr::zip_with_transform([](int x, int y, int z_) { return x + y + z_; }, std::ref(other), std::ref(a));

    EXPECT_RANGE_EQ(z, (std::vector<int>{111, 222, 333, 444, 555}));
    EXPECT_EQ(std::ranges::size(z), 5u);
}

// The pack of the mutable iterator is {int *, int const *} and the pack of the constant iterator is
// {int const *, int const *}. zip_iterator only stores a std::array when all iterators have the same
// type, so the two disagree on storage (std::tuple vs std::array) and the iterator -> const_iterator
// conversion demanded by radr::borrowing_rad has to bridge that.

TEST(zip_with_transform_mp, mixed_constness)
{
    auto z = std::ref(vec) | radr::zip_with_transform(add, std::cref(other));

    EXPECT_RANGE_EQ(z, sums);
    // not constant: the iterator is not const-symmetric, because only one of the inputs is constant
    radr::test::check_adaptor_concepts<decltype(z)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(zip_with_transform_mp, mixed_constness_reversed)
{
    auto z = std::cref(vec) | radr::zip_with_transform(add, std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    radr::test::check_adaptor_concepts<decltype(z)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(zip_with_transform_mp, mixed_constness_heterogeneous_elements)
{
    // both packs are std::tuple-stored here (the element types differ), so this covers the
    // tuple -> tuple flavour of the same conversion
    std::vector<std::string> strs{"aa", "bb", "cc"};
    std::vector<int>         nums{1, 2, 3};
    auto                     z = std::ref(strs) |
             radr::zip_with_transform([](std::string const & s, int i) { return (int)s.size() + i; }, std::cref(nums));

    EXPECT_RANGE_EQ(z, (std::vector<int>{3, 4, 5}));
    radr::test::check_adaptor_concepts<decltype(z)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(zip_with_transform_mp, mixed_constness_mutate)
{
    std::vector<int> v{1, 2, 3};
    std::vector<int> w{10, 20, 30};
    auto             z = std::ref(v) | radr::zip_with_transform(first_ref, std::cref(w));

    // the constant half does not make the mutable half read-only
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(z)>, int &);
    EXPECT_SAME_TYPE(radr::detail::range_const_reference_t<decltype(z)>, int const &);

    for (int & i : z)
        i *= 2;

    EXPECT_RANGE_EQ(v, (std::vector<int>{2, 4, 6}));
    EXPECT_RANGE_EQ(w, (std::vector<int>{10, 20, 30}));
}

TEST(zip_with_transform_mp, size_is_minimum)
{
    std::vector<int> const a{1, 2, 3, 4, 5};
    std::vector<int> const b{10, 20};
    std::vector<int> const c{100, 200, 300, 400};
    auto                   z = std::cref(a) |
             radr::zip_with_transform([](int x, int y, int z_) { return x + y + z_; }, std::cref(b), std::cref(c));

    EXPECT_EQ(std::ranges::size(z), 2u);
    EXPECT_RANGE_EQ(z, (std::vector<int>{111, 222}));
}

TEST(zip_with_transform_mp, call_patterns)
{
    // the functor is not a range, so all three canonical call patterns are unambiguous
    auto piped   = std::ref(vec) | radr::zip_with_transform(add, std::ref(other));
    auto direct  = radr::zip_with_transform(std::ref(vec), add, std::ref(other));
    auto closure = radr::zip_with_transform(add, std::ref(other))(std::ref(vec));

    EXPECT_SAME_TYPE(decltype(piped), decltype(direct));
    EXPECT_SAME_TYPE(decltype(piped), decltype(closure));
    EXPECT_RANGE_EQ(piped, sums);
    EXPECT_RANGE_EQ(direct, sums);
    EXPECT_RANGE_EQ(closure, sums);
}

TEST(zip_with_transform_mp, no_intermediate_tuple)
{
    auto z = std::ref(vec) | radr::zip_with_transform(add, std::ref(other));

    // the functor is invoked N-ary; no std::tuple appears in any associated type
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(z)>, int);
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(z)>, int);
    EXPECT_SAME_TYPE(radr::detail::range_const_reference_t<decltype(z)>, int);
}

TEST(zip_with_transform_mp, reference_returning_functor)
{
    auto z = std::ref(vec) | radr::zip_with_transform(first_ref, std::ref(other));

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(z)>, int &);
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(z)>, int);
    // a reference-returning functor keeps mutability
    radr::test::check_adaptor_concepts<decltype(z)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .mut = true, .borrowed = true});
}

TEST(zip_with_transform_mp, heterogeneous_element_types)
{
    std::vector<std::string> strs{"a", "b", "c"};
    std::vector<int>         nums{1, 2, 3};
    auto                     z =
      std::ref(strs) |
      radr::zip_with_transform([](std::string const & s, int i) { return s + std::to_string(i); }, std::ref(nums));

    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(z)>, std::string);
    EXPECT_RANGE_EQ(z, (std::vector<std::string>{"a1", "b2", "c3"}));
}

TEST(zip_with_transform_mp, mixed_categories)
{
    auto z = std::ref(vec) | radr::zip_with_transform(add, std::ref(lst));

    EXPECT_RANGE_EQ(z, (std::vector<int>{2, 4, 6, 8, 10}));
    // std::list caps the category at bidirectional and prevents common
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::bidi, .sized = true, .borrowed = true});
}

TEST(zip_with_transform_mp, iterator_operations)
{
    auto z  = std::ref(vec) | radr::zip_with_transform(add, std::ref(other));
    auto it = z.begin();

    EXPECT_EQ(*it, 11);
    ++it;
    EXPECT_EQ(*it, 22);
    --it;
    EXPECT_EQ(*it, 11);

    EXPECT_EQ(it[0], 11);
    EXPECT_EQ(it[4], 55);

    it += 2;
    EXPECT_EQ(*it, 33);
    it -= 1;
    EXPECT_EQ(*it, 22);

    EXPECT_EQ(z.end() - z.begin(), 5);
    EXPECT_TRUE(z.begin() < z.end());
}

TEST(zip_with_transform_mp, nested)
{
    auto inner = std::ref(vec) | radr::zip_with_transform(add, std::ref(other));
    auto z     = inner | radr::zip_with_transform([](int i, int j) { return i - j; }, std::ref(vec));

    EXPECT_RANGE_EQ(z, (std::vector<int>{10, 20, 30, 40, 50}));
}

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(zip_with_transform_mp, deep_copy)
{
    using T = decltype(std::vector<int>{1, 2, 3, 4, 5} | radr::zip_with_transform(add, radr::iota(10)));

    T cpy;

    {
        T own = std::vector<int>{1, 2, 3, 4, 5} | radr::zip_with_transform(add, radr::iota(10));
        EXPECT_RANGE_EQ(own, (std::vector<int>{11, 13, 15, 17, 19}));
        cpy = own;
    }

    EXPECT_RANGE_EQ(cpy, (std::vector<int>{11, 13, 15, 17, 19}));
}
