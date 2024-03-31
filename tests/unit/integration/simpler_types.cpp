#include <forward_list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/concepts.hpp>
#include <radr/rad/drop.hpp>
#include <radr/rad/take.hpp>

TEST(simpler_types, string_view)
{
    {
        // the full type of the underlying range is reflected in the adaptor
        std::string const str    = "foobar";
        auto              adapt0 = str | std::views::take(2);
        EXPECT_SAME_TYPE(decltype(adapt0), std::ranges::take_view<std::ranges::ref_view<std::string const>>);
    }

    {
        // a slice of a string constant is just a string_view, so that's what we return
        std::string const str    = "foobar";
        auto              adapt0 = std::ref(str) | radr::take(2);
        EXPECT_SAME_TYPE(decltype(adapt0), std::string_view);
    }
}

TEST(simpler_types, contiguous_it2pointer)
{
    {
        // the full type of the underlying range is reflected in the adaptor
        std::vector<int> container0{1, 2, 3};
        auto             adapt0 = container0 | std::views::take(2);
        EXPECT_SAME_TYPE(decltype(adapt0), std::ranges::take_view<std::ranges::ref_view<std::vector<int>>>);

        // even the size of an array is contained
        std::array<int, 3> container1{1, 2, 3};
        auto               adapt1 = container1 | std::views::take(2);
        EXPECT_SAME_TYPE(decltype(adapt1), (std::ranges::take_view<std::ranges::ref_view<std::array<int, 3>>>));

        // the adaptors have different types
        EXPECT_DIFFERENT_TYPE(decltype(adapt0), decltype(adapt1));
    }

    {
        // our adaptors only contain the iterators and these decay to pointers for contiguous ranges
        std::vector<int> container0{1, 2, 3};
        auto             adapt0 = std::ref(container0) | radr::take(2);
        EXPECT_SAME_TYPE(decltype(adapt0), radr::borrowing_rad<int *>);

        // the type of the underlying range is erased completely
        std::array<int, 3> container1{1, 2, 3};
        auto               adapt1 = std::ref(container1) | radr::take(2);
        EXPECT_SAME_TYPE(decltype(adapt1), radr::borrowing_rad<int *>);

        // thus, both adaptors have the same type
        EXPECT_SAME_TYPE(decltype(adapt0), decltype(adapt1));
    }
}

TEST(simpler_types, folding_slices)
{
    {
        // chained "slicing" adaptors lead to nested templates [contiguous range]
        std::vector<int> container0{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto             adapt0 =
          container0 | std::views::take(10) | std::views::drop(1) | std::views::take(5) | std::views::drop(2);
        EXPECT_SAME_TYPE(decltype(adapt0),
                         std::ranges::drop_view<std::ranges::take_view<
                           std::ranges::drop_view<std::ranges::take_view<std::ranges::ref_view<std::vector<int>>>>>>);

        // chained "slicing" adaptors lead to nested templates [forward range]
        std::forward_list<int> container1{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto                   adapt1 =
          container1 | std::views::take(10) | std::views::drop(1) | std::views::take(5) | std::views::drop(2);
        EXPECT_SAME_TYPE(
          decltype(adapt1),
          std::ranges::drop_view<std::ranges::take_view<
            std::ranges::drop_view<std::ranges::take_view<std::ranges::ref_view<std::forward_list<int>>>>>>);
    }
    {
        // chained "slicing" adaptors are type-erased completely [contiguous range]
        std::vector<int> container0{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto             adapt0 = std::ref(container0) | radr::take(10) | radr::drop(1) | radr::take(5) | radr::drop(2);
        EXPECT_SAME_TYPE(decltype(adapt0), radr::borrowing_rad<int *>);

        // even for forward ranges, the adaptor type does not change after the first take()
        std::forward_list<int> container1{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto                   adapt1 = std::ref(container1) | radr::take(10);
        auto adapt2 = std::ref(container1) | radr::take(10) | radr::drop(1) | radr::take(5) | radr::drop(2);
        EXPECT_SAME_TYPE(decltype(adapt1), decltype(adapt2));
    }
}
