#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/concepts.hpp>
#include <radr/rad/elements.hpp>
#include <radr/range_access.hpp>

#include "radr/rad/to_single_pass.hpp"

// This adaptor is just a special case of transform and thus only has minimal tests

std::vector<std::pair<int, float>> vec{
  {1, 1.0},
  {2, 2.0},
  {3, 3.0}
};
std::vector<std::tuple<int, float, double>> vec2{
  {1, 1.0, 1.0},
  {2, 2.0, 2.0},
  {3, 3.0, 3.0}
};

inline constexpr auto to_rvalue =
  radr::detail::overloaded{[](auto & tuple) -> std::tuple<int &, float const &, double>
{ return std::tie(std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple)); },
                           [](auto const & tuple) -> std::tuple<int const &, float const &, double>
{
    return std::tie(std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple));
}};

TEST(single_pass, reference_t_is_lvalue)
{
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(vec)>, (std::pair<int, float> &));
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(vec)>, (std::pair<int, float>));

    {
        auto el0 = std::ref(vec) | radr::to_single_pass | radr::keys;
        EXPECT_RANGE_EQ(el0, (std::vector{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el0)>, int &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el0)>, int);

        auto el1 = std::ref(vec) | radr::to_single_pass | radr::values;
        EXPECT_RANGE_EQ(el1, (std::vector<float>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el1)>, float &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el1)>, float);
    }
}

TEST(multi_pass, reference_t_is_lvalue)
{
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(vec)>, (std::pair<int, float> &));
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(vec)>, (std::pair<int, float>));

    {
        auto el0 = std::ref(vec) | radr::keys;
        EXPECT_RANGE_EQ(el0, (std::vector{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el0)>, int &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el0)>, int);

        auto el1 = std::ref(vec) | radr::values;
        EXPECT_RANGE_EQ(el1, (std::vector<float>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el1)>, float &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el1)>, float);
    }

    {
        auto const el0 = std::ref(vec) | radr::keys;
        EXPECT_RANGE_EQ(el0, (std::vector{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el0)>, int const &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el0)>, int);

        auto const el1 = std::ref(vec) | radr::values;
        EXPECT_RANGE_EQ(el1, (std::vector<float>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el1)>, float const &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el1)>, float);
    }

    {
        auto el0 = std::cref(vec) | radr::keys;
        EXPECT_RANGE_EQ(el0, (std::vector{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el0)>, int const &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el0)>, int);

        auto el1 = std::cref(vec) | radr::values;
        EXPECT_RANGE_EQ(el1, (std::vector<float>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el1)>, float const &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el1)>, float);
    }
}

TEST(single_pass, reference_t_is_rvalue)
{
    auto vecwrap = std::ref(vec2) | radr::transform(to_rvalue);
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(vecwrap)>, (std::tuple<int &, float const &, double>));
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(vecwrap)>, (std::tuple<int &, float const &, double>));

    {
        auto el0 = vecwrap | radr::to_single_pass | radr::keys;
        EXPECT_RANGE_EQ(el0, (std::vector{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el0)>, int &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el0)>, int);

        auto el1 = vecwrap | radr::to_single_pass | radr::values;
        EXPECT_RANGE_EQ(el1, (std::vector<float>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el1)>, float const &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el1)>, float);

        auto el2 = vecwrap | radr::to_single_pass | radr::elements<2>;
        EXPECT_RANGE_EQ(el2, (std::vector<double>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el2)>, double);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el2)>, double);
    }
}

TEST(multi_pass, reference_t_is_rvalue)
{
    auto vecwrap = std::ref(vec2) | radr::transform(to_rvalue);
    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(vecwrap)>, (std::tuple<int &, float const &, double>));
    EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(vecwrap)>, (std::tuple<int &, float const &, double>));

    {
        auto el0 = vecwrap | radr::keys;
        EXPECT_RANGE_EQ(el0, (std::vector{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el0)>, int &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el0)>, int);

        auto el1 = vecwrap | radr::values;
        EXPECT_RANGE_EQ(el1, (std::vector<float>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el1)>, float const &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el1)>, float);

        auto el2 = vecwrap | radr::elements<2>;
        EXPECT_RANGE_EQ(el2, (std::vector<double>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el2)>, double);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el2)>, double);
    }

    {
        auto const el0 = vecwrap | radr::keys;
        EXPECT_RANGE_EQ(el0, (std::vector{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el0)>, int const &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el0)>, int);

        auto const el1 = vecwrap | radr::values;
        EXPECT_RANGE_EQ(el1, (std::vector<float>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el1)>, float const &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el1)>, float);

        auto const el2 = vecwrap | radr::elements<2>;
        EXPECT_RANGE_EQ(el2, (std::vector<double>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el2)>, double);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el2)>, double);
    }

    {
        auto el0 = std::cref(vecwrap) | radr::keys;
        EXPECT_RANGE_EQ(el0, (std::vector{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el0)>, int const &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el0)>, int);

        auto el1 = std::cref(vecwrap) | radr::values;
        EXPECT_RANGE_EQ(el1, (std::vector<float>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el1)>, float const &);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el1)>, float);

        auto el2 = std::cref(vecwrap) | radr::elements<2>;
        EXPECT_RANGE_EQ(el2, (std::vector<double>{1, 2, 3}));
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(el2)>, double);
        EXPECT_SAME_TYPE(std::ranges::range_value_t<decltype(el2)>, double);
    }
}
