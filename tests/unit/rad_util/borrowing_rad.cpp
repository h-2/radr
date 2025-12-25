#include "radr/rad_util/borrowing_rad.hpp"
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/custom/subborrow.hpp>
#include <radr/standalone/empty_range.hpp>

TEST(borrowing_rad, contiguous_construct)
{
    std::vector<int> vec{1, 3, 3};

    radr::borrowing_rad<int *> rad{vec};
}

TEST(empty_range, members)
{


}

TEST(empty_range, subborrow)
{
    radr::empty_range<int> r;

    auto sub1 = radr::borrow(r);
    EXPECT_SAME_TYPE(decltype(sub1), decltype(r));

    auto sub2 = radr::borrow(std::as_const(r));
    EXPECT_SAME_TYPE(decltype(sub2), radr::empty_range<int const>);

    auto sub3 = radr::subborrow(r, (int *)nullptr, (int *)nullptr);
    EXPECT_SAME_TYPE(decltype(sub3), decltype(r));

    auto sub4 = radr::subborrow(r, (int *)nullptr, (int *)nullptr, 0ul);
    EXPECT_SAME_TYPE(decltype(sub4), decltype(r));
}
