// Test-suite for radr::zip_transform, which currently only forwards to radr::zip_with_transform.
// The behaviour itself is covered by rad/zip_with_transform.cpp; this file only pins the forwarding down.
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

TEST(zip_transform_mp, is_zip_with_transform_with_switched_args)
{
    auto fac = radr::zip_transform(add, std::ref(vec), std::ref(other));
    auto ada = radr::zip_with_transform(std::ref(vec), add, std::ref(other));

    EXPECT_SAME_TYPE(decltype(fac), decltype(ada));
    EXPECT_RANGE_EQ(fac, ada);
    EXPECT_RANGE_EQ(fac, sums);

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

TEST(zip_transform_mp, rvalue_container_as_first_range)
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

TEST(zip_transform_sp, single_pass)
{
    auto z = radr::zip_transform(add, radr::iota_sp(1, 6), std::ref(other));

    EXPECT_RANGE_EQ(z, sums);
}
