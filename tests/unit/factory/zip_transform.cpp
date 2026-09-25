// Test-suite for radr::zip_transform.
// The transformation itself is covered by rad/zip_with_transform.cpp; this file pins down what is specific to
// the factory: it mirrors radr::zip's dispatch, i.e. it borrows if every argument is borrowed and otherwise
// returns a radr::zip_container that owns *every* argument.
// NOTE: deliberately NOT guarded by RADR_FEATURE_ZIP; radr::zip_transform is available in C++20.

#include <functional>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/factory/iota.hpp>
#include <radr/factory/zip_transform.hpp>
#include <radr/rad/to_single_pass.hpp>

using radr::test::range_cat;

inline std::vector<int> vec{1, 2, 3, 4, 5};
inline std::vector<int> other{10, 20, 30, 40, 50};

inline constexpr auto add = [](int a, int b)
{
    return a + b;
};

inline std::vector<int> const sums{11, 22, 33, 44, 55};

// --------------------------------------------------------------------------
// which top-level type came back?
// --------------------------------------------------------------------------

enum class ownership
{
    borrowed,
    container,
    other
};

template <typename Iter, typename Sent, typename CIter, typename CSent, auto Kind>
constexpr ownership check_ownership(radr::borrowing_rad<Iter, Sent, CIter, CSent, Kind> const &)
{
    return ownership::borrowed;
}

template <typename Fn, typename... URanges>
constexpr ownership check_ownership(radr::zip_container<radr::zip_policy_transform<Fn>, URanges...> const &)
{
    return ownership::container;
}

constexpr ownership check_ownership(auto const &)
{
    return ownership::other;
}

/* Expectations for the owning results */
inline constexpr radr::test::concept_expectations owning_expectations{.cat    = range_cat::ra,
                                                                      .sized  = true,
                                                                      .common = true};

// --------------------------------------------------------------------------
// all arguments borrowed -> same as the adaptor
// --------------------------------------------------------------------------

TEST(zip_transform_mp, is_zip_with_transform_with_switched_args)
{
    // only guaranteed while every argument is borrowed; see owns_every_rvalue_container below
    auto fac = radr::zip_transform(add, std::ref(vec), std::ref(other));
    auto ada = radr::zip_with_transform(std::ref(vec), add, std::ref(other));

    EXPECT_SAME_TYPE(decltype(fac), decltype(ada));
    EXPECT_RANGE_EQ(fac, ada);
    EXPECT_RANGE_EQ(fac, sums);
    EXPECT_EQ(check_ownership(fac), ownership::borrowed);

    radr::test::check_adaptor_concepts<decltype(fac)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

TEST(zip_transform_mp, single_range)
{
    auto z = radr::zip_transform([](int i) { return i * 2; }, std::ref(vec));

    EXPECT_RANGE_EQ(z, (std::vector<int>{2, 4, 6, 8, 10}));
}

TEST(zip_transform_mp, three_ranges)
{
    std::vector<int> a{100, 200, 300, 400, 500};

    auto z =
      radr::zip_transform([](int x, int y, int z_) { return x + y + z_; }, std::ref(vec), std::ref(other), std::ref(a));

    EXPECT_RANGE_EQ(z, (std::vector<int>{111, 222, 333, 444, 555}));
}

TEST(zip_transform_mp, bidi)
{
    std::list<int> lst{1, 2, 3, 4, 5};

    auto z = radr::zip_transform(add, std::ref(lst), std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    // both inputs are sized, but all inputs are bidirectional and std::list is not indexable -> not common
    radr::test::check_adaptor_concepts<decltype(z)>({.cat = range_cat::bidi, .sized = true, .borrowed = true});
}

TEST(zip_transform_mp, mixed_constness)
{
    // mutable and constant ranges over the same element type; see rad/zip_with_transform.cpp for the
    // storage mismatch this exercises in the iterator -> const_iterator conversion
    auto z = radr::zip_transform(add, std::ref(vec), std::cref(other));

    EXPECT_RANGE_EQ(z, sums);
    radr::test::check_adaptor_concepts<decltype(z)>(
      {.cat = range_cat::ra, .sized = true, .common = true, .borrowed = true});
}

// --------------------------------------------------------------------------
// at least one container -> owning radr::zip_container
// --------------------------------------------------------------------------

TEST(zip_transform_mp, rvalue_container_as_first_range)
{
    auto z = radr::zip_transform(add, std::vector<int>{1, 2, 3, 4, 5}, std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
    EXPECT_EQ(check_ownership(z), ownership::container);
    radr::test::check_adaptor_concepts<decltype(z)>(owning_expectations);
}

TEST(zip_transform_mp, rvalue_container_as_other_range)
{
    // this is what radr::zip_with_transform rejects and what the factory adds on top of it
    auto z = radr::zip_transform(add, std::ref(vec), std::vector<int>{10, 20, 30, 40, 50});

    EXPECT_RANGE_EQ(z, sums);
    EXPECT_EQ(check_ownership(z), ownership::container);
    radr::test::check_adaptor_concepts<decltype(z)>(owning_expectations);
}

TEST(zip_transform_mp, owns_every_rvalue_container)
{
    auto z = radr::zip_transform(add, std::vector<int>{1, 2, 3, 4, 5}, std::vector<int>{10, 20, 30, 40, 50});

    EXPECT_RANGE_EQ(z, sums);
    EXPECT_EQ(check_ownership(z), ownership::container);
    radr::test::check_adaptor_concepts<decltype(z)>(owning_expectations);

    // and it is now distinct from what the adaptor returns for the borrowed form
    auto ada = radr::zip_with_transform(std::ref(vec), add, std::ref(other));
    EXPECT_FALSE((std::same_as<decltype(z), decltype(ada)>));
}

TEST(zip_transform_mp, three_rvalue_containers)
{
    auto z = radr::zip_transform([](int x, int y, int z_) { return x + y + z_; },
                                 std::vector<int>{1, 2, 3, 4, 5},
                                 std::vector<int>{10, 20, 30, 40, 50},
                                 std::vector<int>{100, 200, 300, 400, 500});

    EXPECT_RANGE_EQ(z, (std::vector<int>{111, 222, 333, 444, 555}));
    EXPECT_EQ(check_ownership(z), ownership::container);
    radr::test::check_adaptor_concepts<decltype(z)>(owning_expectations);
}

TEST(zip_transform_mp, shorter_range_wins)
{
    auto z = radr::zip_transform(add, std::vector<int>{1, 2, 3}, std::vector<int>{10, 20, 30, 40, 50});

    EXPECT_RANGE_EQ(z, (std::vector<int>{11, 22, 33}));
    EXPECT_EQ(std::ranges::size(z), 3u);
}

TEST(zip_transform_mp, empty)
{
    auto z = radr::zip_transform(add, std::vector<int>{}, std::vector<int>{});

    EXPECT_RANGE_EQ(z, (std::vector<int>{}));
    EXPECT_TRUE(z.begin() == z.end());
}

TEST(zip_transform_mp, deep_copy)
{
    using T = decltype(radr::zip_transform(add, std::vector<int>{1, 2, 3, 4, 5}, std::ref(other)));

    T cpy;

    {
        T own = radr::zip_transform(add, std::vector<int>{1, 2, 3, 4, 5}, std::ref(other));
        EXPECT_RANGE_EQ(own, sums);
        cpy = own;
    }

    EXPECT_RANGE_EQ(cpy, sums);
}

TEST(zip_transform_mp, deep_copy_all_owning)
{
    using T = decltype(radr::zip_transform(add, std::vector<int>{1, 2, 3}, std::vector<int>{10, 20, 30}));

    T cpy;

    {
        T own = radr::zip_transform(add, std::vector<int>{1, 2, 3}, std::vector<int>{10, 20, 30});
        EXPECT_RANGE_EQ(own, (std::vector<int>{11, 22, 33}));
        cpy = own;
    }

    EXPECT_RANGE_EQ(cpy, (std::vector<int>{11, 22, 33}));
}
