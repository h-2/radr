#include <cstddef>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/custom/subborrow.hpp>
#include <radr/factory/repeat.hpp>
#include <radr/rad/slice.hpp>

//---------------------------------------------------------------------
// deduction guides
//---------------------------------------------------------------------

TEST(repeat, deduction_guides)
{
    int i = 3;
    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(std::ref(i))),
                     (radr::repeat_rng<int, std::unreachable_sentinel_t, radr::repeat_rng_storage::indirect>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(std::ref(i), std::unreachable_sentinel)),
                     (radr::repeat_rng<int, std::unreachable_sentinel_t, radr::repeat_rng_storage::indirect>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(std::ref(i), 5)),
                     (radr::repeat_rng<int, ptrdiff_t, radr::repeat_rng_storage::indirect>));

    EXPECT_SAME_TYPE(
      decltype(radr::repeat_rng(std::vector{3})),
      (radr::repeat_rng<std::vector<int>, std::unreachable_sentinel_t, radr::repeat_rng_storage::in_range>));

    EXPECT_SAME_TYPE(
      decltype(radr::repeat_rng(std::vector{3}, std::unreachable_sentinel)),
      (radr::repeat_rng<std::vector<int>, std::unreachable_sentinel_t, radr::repeat_rng_storage::in_range>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(std::vector{3}, 5)),
                     (radr::repeat_rng<std::vector<int>, ptrdiff_t, radr::repeat_rng_storage::in_range>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(3)),
                     (radr::repeat_rng<int const, std::unreachable_sentinel_t, radr::repeat_rng_storage::in_iterator>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(3, std::unreachable_sentinel)),
                     (radr::repeat_rng<int const, std::unreachable_sentinel_t, radr::repeat_rng_storage::in_iterator>));

    EXPECT_SAME_TYPE(decltype(radr::repeat_rng(3, 5)),
                     (radr::repeat_rng<int const, ptrdiff_t, radr::repeat_rng_storage::in_iterator>));
}

template <typename repeat_rng_t>
struct repeat_rng : public testing::Test
{};

enum class rsize
{
    dynamic_,
    static_,
    infinite_
};

template <typename TVal_, typename Bound_, radr::repeat_rng_storage storage_>
struct repeat_rng<radr::repeat_rng<TVal_, Bound_, storage_>> : public testing::Test
{
    using Rng                     = radr::repeat_rng<TVal_, Bound_, storage_>;
    using TVal                    = TVal_;
    using Bound                   = Bound_;
    static constexpr auto storage = storage_;

    static constexpr bool borrowed = storage != radr::repeat_rng_storage::in_range;
    static constexpr bool is_const = std::is_const_v<TVal> || storage == radr::repeat_rng_storage::in_iterator;
    using TValC                    = std::conditional_t<is_const, TVal const, TVal>;
    static constexpr rsize size    = std::same_as<Bound, ptrdiff_t>                     ? rsize::dynamic_
                                     : std::same_as<Bound, std::unreachable_sentinel_t> ? rsize::infinite_
                                                                                        : rsize::static_;
    static auto            get_bound()
    {
        if constexpr (size == rsize::dynamic_)
            return ptrdiff_t(5);
        else
            return Bound{};
    }

    using subborrow_t = std::conditional_t<borrowed,
                                           radr::repeat_rng<TValC, ptrdiff_t, storage>,
                                           radr::borrowing_rad<radr::iterator_t<Rng>,
                                                               radr::iterator_t<Rng>,
                                                               radr::const_iterator_t<Rng>,
                                                               radr::const_iterator_t<Rng>,
                                                               radr::borrowing_rad_kind::sized>>;

    using infinite_subborrow_t = std::conditional_t<borrowed,
                                                    radr::repeat_rng<TValC, std::unreachable_sentinel_t, storage>,
                                                    radr::borrowing_rad<radr::iterator_t<Rng>,
                                                                        std::unreachable_sentinel_t,
                                                                        radr::const_iterator_t<Rng>,
                                                                        std::unreachable_sentinel_t,
                                                                        radr::borrowing_rad_kind::unsized>>;
};

using rng_types =
  ::testing::Types</* dynamic size */
                   radr::repeat_rng<int, ptrdiff_t, radr::repeat_rng_storage::indirect>,
                   radr::repeat_rng<int, ptrdiff_t, radr::repeat_rng_storage::in_range>,
                   radr::repeat_rng<int, ptrdiff_t, radr::repeat_rng_storage::in_iterator>,
                   radr::repeat_rng<int const, ptrdiff_t, radr::repeat_rng_storage::indirect>,
                   radr::repeat_rng<int const, ptrdiff_t, radr::repeat_rng_storage::in_range>,
                   radr::repeat_rng<int const, ptrdiff_t, radr::repeat_rng_storage::in_iterator>,
                   /* constant size */
                   radr::repeat_rng<int, radr::constant_t<5>, radr::repeat_rng_storage::indirect>,
                   radr::repeat_rng<int, radr::constant_t<5>, radr::repeat_rng_storage::in_range>,
                   radr::repeat_rng<int, radr::constant_t<5>, radr::repeat_rng_storage::in_iterator>,
                   radr::repeat_rng<int const, radr::constant_t<5>, radr::repeat_rng_storage::indirect>,
                   radr::repeat_rng<int const, radr::constant_t<5>, radr::repeat_rng_storage::in_range>,
                   radr::repeat_rng<int const, radr::constant_t<5>, radr::repeat_rng_storage::in_iterator>,
                   /* infinite size */
                   radr::repeat_rng<int, std::unreachable_sentinel_t, radr::repeat_rng_storage::in_range>,
                   radr::repeat_rng<int, std::unreachable_sentinel_t, radr::repeat_rng_storage::indirect>,
                   radr::repeat_rng<int, std::unreachable_sentinel_t, radr::repeat_rng_storage::in_iterator>,
                   radr::repeat_rng<int const, std::unreachable_sentinel_t, radr::repeat_rng_storage::in_range>,
                   radr::repeat_rng<int const, std::unreachable_sentinel_t, radr::repeat_rng_storage::indirect>,
                   radr::repeat_rng<int const, std::unreachable_sentinel_t, radr::repeat_rng_storage::in_iterator>>;

TYPED_TEST_SUITE(repeat_rng, rng_types);

TYPED_TEST(repeat_rng, concepts)
{
    using R = TypeParam;

    EXPECT_TRUE(std::ranges::random_access_range<R>);
    EXPECT_EQ(std::ranges::borrowed_range<R>, TestFixture::borrowed);
    EXPECT_EQ(std::ranges::common_range<R>, TestFixture::size != rsize::infinite_);
    EXPECT_EQ(std::ranges::sized_range<R>, TestFixture::size != rsize::infinite_);

    if constexpr (TestFixture::storage == radr::repeat_rng_storage::in_iterator)
    {
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<R>, int);
    }
    else if constexpr (TestFixture::is_const)
    {
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<R>, int const &);
    }
    else
    {
        EXPECT_SAME_TYPE(std::ranges::range_reference_t<R>, int &);
    }

    // not an adaptor, but fulfils all the requirements
    radr::test::generic_adaptor_checks<R, std::vector<int> /*irrelevant*/>();
}

TYPED_TEST(repeat_rng, no_arg)
{
    TypeParam r{};

    if constexpr (TestFixture::size == rsize::dynamic_)
    {
        EXPECT_EQ(r.size(), 0ull);
        EXPECT_EQ(r.empty(), true);
    }
    else if constexpr (TestFixture::storage == radr::repeat_rng_storage::indirect)
    {
        /* this is the only dangerous case */
        EXPECT_EQ(r.empty(), false);
        // deref would be UB
    }
    else
    {
        EXPECT_EQ(r.empty(), false);
        EXPECT_EQ(*r.begin(), 0);
        EXPECT_EQ(r[2], 0);
        if constexpr (TestFixture::size == rsize::static_)
        {
            EXPECT_EQ(r.size(), 5);
        }
    }
}

TYPED_TEST(repeat_rng, one_arg)
{
    int       i = 42;
    TypeParam r{i};

    if constexpr (TestFixture::size == rsize::dynamic_)
    {
        EXPECT_EQ(r.size(), 0ull);
        EXPECT_EQ(r.empty(), true);
    }
    else if constexpr (TestFixture::size == rsize::static_)
    {
        EXPECT_EQ(r.empty(), false);
        EXPECT_EQ(*r.begin(), 42);
        EXPECT_EQ(r[2], 42);
        EXPECT_EQ(r.size(), 5);
    }
    else
    {
        EXPECT_EQ(r.empty(), false);
        EXPECT_EQ(*r.begin(), 42);
        EXPECT_EQ(r[2], 42);
    }
}

TYPED_TEST(repeat_rng, two_arg)
{
    int i = 3;

    if constexpr (TestFixture::size == rsize::dynamic_)
    {
        TypeParam r{i, 5};
        EXPECT_EQ(r.size(), 5ull);
        EXPECT_EQ(r.empty(), false);
        EXPECT_EQ(*r.begin(), 3);
        EXPECT_EQ(r[2], 3);

        if constexpr (!TestFixture::is_const)
        {
            r[2] = 42; // changing one value, changes all
            EXPECT_EQ(*r.begin(), 42);
        }
    }
    else if constexpr (TestFixture::size == rsize::static_)
    {
        TypeParam r{i, radr::constant<5>};
        EXPECT_EQ(r.size(), 5ull);
        EXPECT_EQ(r.empty(), false);
        EXPECT_EQ(*r.begin(), 3);
        EXPECT_EQ(r[2], 3);

        if constexpr (!TestFixture::is_const)
        {
            r[2] = 42; // changing one value, changes all
            EXPECT_EQ(*r.begin(), 42);
        }
    }
    else
    {
        TypeParam r{i, std::unreachable_sentinel};
        EXPECT_EQ(r.empty(), false);
        EXPECT_EQ(*r.begin(), 3);
        EXPECT_EQ(r[2], 3);

        if constexpr (!TestFixture::is_const)
        {
            r[2] = 42; // changing one value, changes all
            EXPECT_EQ(*r.begin(), 42);
        }
    }
}

TYPED_TEST(repeat_rng, iterators)
{
    int i = 3;

    if constexpr (TestFixture::size != rsize::infinite_)
    {
        TypeParam r{i, radr::constant<5>};

        auto it = r.begin();
        it += 4;
        auto it2 = r.end();
        --it2;

        EXPECT_EQ(it, it2);
        EXPECT_EQ(r.end() - r.begin(), 5);
        EXPECT_EQ(r.end() - it, 1);

        ++it2;
        EXPECT_EQ(it2, r.end());
    }
    else
    {
        TypeParam r{i};

        auto it  = r.begin();
        auto it2 = it + 100;

        EXPECT_NE(it, it2);
        EXPECT_EQ(it2 - it, 100);

        it++;
        it2--;
        EXPECT_EQ(it2 - it, 98);
    }
}

TYPED_TEST(repeat_rng, subborrow_two_it)
{
    int i = 3;

    TypeParam r{i, TestFixture::get_bound()};

    auto sub = radr::subborrow(r, r.begin() + 1, r.begin() + 3);

    EXPECT_SAME_TYPE(decltype(sub), typename TestFixture::subborrow_t);
    EXPECT_EQ(sub.size(), 2);
    EXPECT_EQ(sub[0], 3);
    if constexpr (!TestFixture::is_const)
    {
        sub[0] = 42;
        EXPECT_EQ(sub[1], 42);
        if constexpr (TestFixture::storage == radr::repeat_rng_storage::in_range)
        {
            EXPECT_EQ(r[1], 42); // in-range storage changed (because in_range does not get tag_invoked()
            EXPECT_EQ(i, 3);     // extneral storage unaffected
        }
        else // radr::repeat_rng_storage::indirect)
        {
            EXPECT_EQ(r[1], 42);  // original changed
            EXPECT_EQ(i, 42);     // extneral storage changed
            EXPECT_EQ(&r[i], &i); // pointer preserved
        }
    }
}

TYPED_TEST(repeat_rng, subborrow_size)
{
    int i = 3;

    TypeParam r{i, TestFixture::get_bound()};

    auto sub = radr::subborrow(r, r.begin() + 1, r.begin() + 3, 2ull); // only line different from above

    EXPECT_SAME_TYPE(decltype(sub), typename TestFixture::subborrow_t);
    EXPECT_EQ(sub.size(), 2);
    EXPECT_EQ(sub[0], 3);
    if constexpr (!TestFixture::is_const)
    {
        sub[0] = 42;
        EXPECT_EQ(sub[1], 42);
        if constexpr (TestFixture::storage == radr::repeat_rng_storage::in_range)
        {
            EXPECT_EQ(r[1], 42); // in-range storage changed (because in_range does not get tag_invoked()
            EXPECT_EQ(i, 3);     // extneral storage unaffected
        }
        else // radr::repeat_rng_storage::indirect)
        {
            EXPECT_EQ(r[1], 42);  // original changed
            EXPECT_EQ(i, 42);     // extneral storage changed
            EXPECT_EQ(&r[i], &i); // pointer preserved
        }
    }
}

TYPED_TEST(repeat_rng, subborrow_infinite)
{
    int i = 3;

    TypeParam r{i, TestFixture::get_bound()};

    auto sub = radr::subborrow(r, r.begin() + 1, std::unreachable_sentinel);

    EXPECT_SAME_TYPE(decltype(sub), typename TestFixture::infinite_subborrow_t);
    EXPECT_FALSE(std::ranges::sized_range<decltype(sub)>);
    EXPECT_EQ(sub[0], 3);
    if constexpr (!TestFixture::is_const)
    {
        sub[0] = 42;
        EXPECT_EQ(sub[1], 42);
        if constexpr (TestFixture::storage == radr::repeat_rng_storage::in_range)
        {
            EXPECT_EQ(r[1], 42); // in-range storage changed (because in_range does not get tag_invoked()
            EXPECT_EQ(i, 3);     // extneral storage unaffected
        }
        else // radr::repeat_rng_storage::indirect)
        {
            EXPECT_EQ(r[1], 42);  // original changed
            EXPECT_EQ(i, 42);     // extneral storage changed
            EXPECT_EQ(&r[i], &i); // pointer preserved
        }
    }
}

TYPED_TEST(repeat_rng, subborrow_infinite_size)
{
    if constexpr (TestFixture::borrowed)
    {
        int i = 3;

        TypeParam r{i, TestFixture::get_bound()};

        auto sub = radr::subborrow(r, r.begin() + 1, std::unreachable_sentinel, 2);

        EXPECT_SAME_TYPE(decltype(sub), typename TestFixture::infinite_subborrow_t);
        EXPECT_FALSE(std::ranges::sized_range<decltype(sub)>);
        EXPECT_EQ(sub[0], 3);
        if constexpr (!TestFixture::is_const)
        {
            sub[0] = 42;
            EXPECT_EQ(sub[1], 42);

            EXPECT_EQ(r[1], 42);  // original changed
            EXPECT_EQ(i, 42);     // extneral storage changed
            EXPECT_EQ(&r[i], &i); // pointer preserved
        }
    }
}
