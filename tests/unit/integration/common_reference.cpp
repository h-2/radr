#include <concepts>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/gtest_helpers.hpp>

#include <radr/class/borrowing_rad.hpp>
#include <radr/concepts.hpp>
#include <radr/rad/chunk.hpp>
#include <radr/rad/chunk_by.hpp>
#include <radr/rad/split.hpp>

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

// a "mutable" borrowing_rad: Iter != CIter
using BR = radr::borrowing_rad<int *>;
static_assert(!std::same_as<radr::iterator_t<BR>, radr::const_iterator_t<BR>>);

// an already-"const" borrowing_rad: Iter == CIter
using CBR = radr::borrowing_rad<int const *>;
static_assert(std::same_as<radr::iterator_t<CBR>, radr::const_iterator_t<CBR>>);

// --------------------------------------------------------------------------
// std::common_reference_t: mutable borrowing_rad + a const-qualification of itself -> CBR
// --------------------------------------------------------------------------

TEST(common_reference, mutable_borrowing_rad_collapses_to_const)
{
    // this is exactly the shape radr::detail::iter_const_reference_t computes
    EXPECT_SAME_TYPE((std::common_reference_t<BR const &&, BR>), CBR);
    // order must not matter
    EXPECT_SAME_TYPE((std::common_reference_t<BR, BR const &&>), CBR);

    // other qualifier combinations that also carry "const" on one side
    EXPECT_SAME_TYPE((std::common_reference_t<BR const &, BR>), CBR);
    EXPECT_SAME_TYPE((std::common_reference_t<BR, BR const &>), CBR);
}

TEST(common_reference, mutable_borrowing_rad_without_const_stays_mutable)
{
    // neither side const-qualified -> no reason to collapse
    EXPECT_SAME_TYPE((std::common_reference_t<BR, BR>), BR);
    // BR& and BR&& resolve via COMMON-REF (both are reference types, our specialization is
    // never consulted); the usual lvalue/rvalue-ref unification rules apply, landing on BR const&.
    EXPECT_SAME_TYPE((std::common_reference_t<BR &, BR &&>), BR const &);
}

TEST(common_reference, already_const_borrowing_rad_stays_itself)
{
    // Iter == CIter already, so "collapsing to const" is a no-op: both branches of the
    // specialization's conditional produce the identical type.
    EXPECT_SAME_TYPE((std::common_reference_t<CBR const &&, CBR>), CBR);
    EXPECT_SAME_TYPE((std::common_reference_t<CBR, CBR>), CBR);
}

// --------------------------------------------------------------------------
// std::common_reference_with: symmetry + convertibility across qualifiers
// --------------------------------------------------------------------------

TEST(common_reference, common_reference_with_mutable_and_const_qualified)
{
    EXPECT_TRUE((std::common_reference_with<BR, BR const &>));
    EXPECT_TRUE((std::common_reference_with<BR const &, BR>));
    EXPECT_TRUE((std::common_reference_with<BR, BR const &&>));
    EXPECT_TRUE((std::common_reference_with<BR const &&, BR>));
}

TEST(common_reference, common_reference_with_unaffected_reference_combinations)
{
    // both operands are already reference types -> resolved via COMMON-REF directly,
    // without ever consulting our basic_common_reference specialization; must still hold.
    EXPECT_TRUE((std::common_reference_with<BR &, BR &&>));
    EXPECT_TRUE((std::common_reference_with<BR &&, BR &>));
    EXPECT_TRUE((std::common_reference_with<BR const &, BR &>));
}

TEST(common_reference, common_reference_with_already_const)
{
    EXPECT_TRUE((std::common_reference_with<CBR, CBR const &>));
    EXPECT_TRUE((std::common_reference_with<CBR const &, CBR>));
}

// --------------------------------------------------------------------------
// practical effect: radr::constant_iterator / radr::constant_range on real adaptors
// --------------------------------------------------------------------------
//
// radr::split, radr::chunk and radr::chunk_by all yield an outer range whose iterator
// dereferences to a prvalue radr::borrowing_rad (rather than a reference). Before the
// basic_common_reference specialization above, radr::constant_range<outer_t const> was
// always false for these -- even for the const-borrow variant, which cannot mutate
// anything. These tests pin down that the fix actually reaches that concept.

TEST(common_reference, chunk_outer_range_is_constant_range_when_const)
{
    std::vector<int> v{1, 2, 3, 4, 5};
    auto             ra = std::ref(v) | radr::chunk(2);
    using T             = decltype(ra);

    // the mutable outer range still permits assign-through -> correctly NOT constant_range
    EXPECT_FALSE(radr::constant_range<T>);
    // the const-qualified outer range's iterator (== the const-borrow variant) cannot mutate
    // anything -> now correctly recognized as constant_range
    EXPECT_TRUE(radr::constant_range<T const>);
}

TEST(common_reference, chunk_by_outer_range_is_constant_range_when_const)
{
    std::vector<int> v{1, 2, 3, 4, 5};
    auto             ra = std::ref(v) | radr::chunk_by([](int a, int b) { return a < b; });
    using T             = decltype(ra);

    EXPECT_FALSE(radr::constant_range<T>);
    EXPECT_TRUE(radr::constant_range<T const>);
}

TEST(common_reference, split_outer_range_is_constant_range_when_const)
{
    std::vector<int> v{1, 2, 3, 4, 5};
    auto             ra = std::ref(v) | radr::split(3);
    using T             = decltype(ra);

    EXPECT_FALSE(radr::constant_range<T>);
    EXPECT_TRUE(radr::constant_range<T const>);
}
