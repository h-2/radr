#include <gtest/gtest.h>

#include <radr/version.hpp>

#if !RADR_FEATURE_ZIP

TEST(dummy, skipped_because_no_cpp23)
{
    GTEST_SKIP() << "Requires C++23";
}

#else

#    include <array>
#    include <deque>
#    include <forward_list>
#    include <list>
#    include <memory>
#    include <ranges>
#    include <string>
#    include <tuple>
#    include <vector>

#    include <radr/test/gtest_helpers.hpp>

#    include <radr/factory/iota.hpp>
#    include <radr/factory/zip.hpp>
#    include <radr/rad/take.hpp>

// created with help of AI

enum class rad_type : uint8_t
{
    other,
    borrowing_rad,
    owning_rad,
    zip_rng,
    generator
};

constexpr rad_type check_rng_type(auto const &)
{
    return rad_type::other;
}
template <typename Iter, typename Sent, typename CIter, typename CSent, auto Kind>
constexpr rad_type check_rad_type(radr::borrowing_rad<Iter, Sent, CIter, CSent, Kind> const &)
{
    return rad_type::borrowing_rad;
}
template <typename URange, typename BorrowingRange>
constexpr rad_type check_rad_type(radr::owning_rad<URange, BorrowingRange> const &)
{
    return rad_type::owning_rad;
}
template <typename... URanges>
constexpr rad_type check_rad_type(radr::zip_rng<URanges...> const &)
{
    return rad_type::zip_rng;
}
template <typename Ref, typename Val>
constexpr rad_type check_rad_type(radr::generator<Ref, Val> const &)
{
    return rad_type::generator;
}

//===========================================================================
// radr::zip - Multi-pass version
//===========================================================================

// ---------- Basic functionality ----------

TEST(zip, SingleRange)
{
    std::vector<int> vec{1, 2, 3};
    auto             z = radr::zip(std::ref(vec));

    std::vector<std::tuple<int>> expected{{1}, {2}, {3}};
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, TwoRanges)
{
    std::vector<int>  a{1, 2, 3};
    std::vector<char> b{'a', 'b', 'c'};
    auto              z = radr::zip(std::ref(a), std::ref(b));

    std::vector<std::tuple<int, char>> expected{
      {1, 'a'},
      {2, 'b'},
      {3, 'c'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, ThreeRanges)
{
    std::vector<int>    a{1, 2, 3};
    std::vector<char>   b{'x', 'y', 'z'};
    std::vector<double> c{1.1, 2.2, 3.3};
    auto                z = radr::zip(std::ref(a), std::ref(b), std::ref(c));

    std::vector<std::tuple<int, char, double>> expected{
      {1, 'x', 1.1},
      {2, 'y', 2.2},
      {3, 'z', 3.3}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, RvalueContainer)
{
    auto z = radr::zip(std::vector<int>{10, 20, 30});

    std::vector<std::tuple<int>> expected{{10}, {20}, {30}};
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_TRUE(std::ranges::common_range<decltype(z)>);
    EXPECT_EQ(check_rad_type(z), rad_type::zip_rng);
}

TEST(zip, MultipleRvalueContainers)
{
    auto z = radr::zip(std::vector<int>{1, 2, 3}, std::vector<char>{'a', 'b', 'c'});

    std::vector<std::tuple<int, char>> expected{
      {1, 'a'},
      {2, 'b'},
      {3, 'c'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_TRUE(std::ranges::common_range<decltype(z)>);
    EXPECT_EQ(check_rad_type(z), rad_type::zip_rng);
}

TEST(zip, MixedRvalueAndRef)
{
    std::vector<char> chars{'x', 'y', 'z'};
    auto              z = radr::zip(std::vector<int>{1, 2, 3}, std::ref(chars));

    std::vector<std::tuple<int, char>> expected{
      {1, 'x'},
      {2, 'y'},
      {3, 'z'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_TRUE(std::ranges::common_range<decltype(z)>);
    EXPECT_EQ(check_rad_type(z), rad_type::zip_rng);
}

// ---------- Edge cases ----------

TEST(zip, EmptyRanges)
{
    std::vector<int> a{};
    std::vector<int> b{};
    auto             z = radr::zip(std::ref(a), std::ref(b));

    EXPECT_EQ(std::ranges::size(z), 0u);
    EXPECT_TRUE(z.begin() == z.end());
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, OneEmptyRange)
{
    std::vector<int> a{};
    std::vector<int> b{1, 2, 3};
    auto             z = radr::zip(std::ref(a), std::ref(b));

    EXPECT_EQ(std::ranges::size(z), 0u);
    EXPECT_TRUE(z.begin() == z.end());
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, DifferentLengths)
{
    std::vector<int>  a{1, 2, 3, 4, 5};
    std::vector<char> b{'a', 'b'};
    auto              z = radr::zip(std::ref(a), std::ref(b));

    std::vector<std::tuple<int, char>> expected{
      {1, 'a'},
      {2, 'b'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(std::ranges::size(z), 2u);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, DifferentLengthsThreeRanges)
{
    std::vector<int>    a{1, 2, 3, 4};
    std::vector<char>   b{'a', 'b', 'c'};
    std::vector<double> c{1.1, 2.2, 3.3, 4.4, 5.5};
    auto                z = radr::zip(std::ref(a), std::ref(b), std::ref(c));

    EXPECT_EQ(std::ranges::size(z), 3u);
    std::vector<std::tuple<int, char, double>> expected{
      {1, 'a', 1.1},
      {2, 'b', 2.2},
      {3, 'c', 3.3}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, SingleElement)
{
    std::vector<int>  a{42};
    std::vector<char> b{'x'};
    auto              z = radr::zip(std::ref(a), std::ref(b));

    std::vector<std::tuple<int, char>> expected{
      {42, 'x'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

// ---------- Reference and value semantics ----------

TEST(zip, ReferencesAreMutable)
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{10, 20, 30};

    auto z = radr::zip(std::ref(a), std::ref(b));

    for (auto [x, y] : z)
    {
        x *= 2;
        y += 5;
    }

    EXPECT_EQ(a, (std::vector<int>{2, 4, 6}));
    EXPECT_EQ(b, (std::vector<int>{15, 25, 35}));
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, ConstReferencesNotMutable)
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{4, 5, 6};

    auto z = radr::zip(std::cref(a), std::cref(b));

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(z)>, (std::tuple<int const &, int const &>));
    EXPECT_SAME_TYPE(radr::detail::range_const_reference_t<decltype(z)>, (std::tuple<int const &, int const &>));
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);

    int sum = 0;
    for (auto [x, y] : z)
    {
        sum += x + y;
    }
    EXPECT_EQ(sum, (1 + 4) + (2 + 5) + (3 + 6));
}

TEST(zip, ConstReferencesNotMutable2)
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{4, 5, 6};

    auto const z = radr::zip(std::ref(a), std::ref(b));

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(z)>, (std::tuple<int const &, int const &>));
    EXPECT_SAME_TYPE(radr::detail::range_const_reference_t<decltype(z)>, (std::tuple<int const &, int const &>));
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);

    int sum = 0;
    for (auto [x, y] : z)
    {
        sum += x + y;
    }
    EXPECT_EQ(sum, (1 + 4) + (2 + 5) + (3 + 6));
}

TEST(zip, ReferenceTypesAreTuples)
{
    std::vector<int>         xs{1, 2, 3};
    std::vector<std::string> ys{"a", "b", "c"};

    auto z = radr::zip(std::ref(xs), std::ref(ys));

    using Ref      = std::ranges::range_reference_t<decltype(z)>;
    using ConstRef = radr::detail::range_const_reference_t<decltype(z)>;
    EXPECT_SAME_TYPE(Ref, (std::tuple<int &, std::string &>));
    EXPECT_SAME_TYPE(ConstRef, (std::tuple<int const &, std::string const &>));
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);

    auto it     = z.begin();
    auto [i, s] = *it;
    EXPECT_EQ(i, 1);
    EXPECT_EQ(s, "a");

    i = 100;
    s = "modified";
    EXPECT_EQ(xs[0], 100);
    EXPECT_EQ(ys[0], "modified");
}

TEST(zip, MoveOnlyElements)
{
    std::vector<std::unique_ptr<int>> ptrs;
    ptrs.emplace_back(std::make_unique<int>(1));
    ptrs.emplace_back(std::make_unique<int>(2));
    ptrs.emplace_back(std::make_unique<int>(3));

    std::vector<int> multipliers{10, 20, 30};

    auto z = radr::zip(std::ref(ptrs), std::ref(multipliers));

    EXPECT_SAME_TYPE(std::ranges::range_reference_t<decltype(z)>, (std::tuple<std::unique_ptr<int> &, int &>));
    EXPECT_SAME_TYPE(radr::detail::range_const_reference_t<decltype(z)>,
                     (std::tuple<std::unique_ptr<int> const &, int const &>));
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);

    for (auto [ptr, mult] : z)
    {
        *ptr *= mult;
    }

    EXPECT_EQ(*ptrs[0], 10);
    EXPECT_EQ(*ptrs[1], 40);
    EXPECT_EQ(*ptrs[2], 90);
}

// ---------- Different container types ----------

TEST(zip, VectorAndArray)
{
    std::vector<int>    vec{1, 2, 3};
    std::array<char, 3> arr{'a', 'b', 'c'};

    auto z = radr::zip(std::ref(vec), std::ref(arr));

    std::vector<std::tuple<int, char>> expected{
      {1, 'a'},
      {2, 'b'},
      {3, 'c'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, VectorAndList)
{
    std::vector<int> vec{1, 2, 3};
    std::list<char>  lst{'x', 'y', 'z'};

    auto z = radr::zip(std::ref(vec), std::ref(lst));

    std::vector<std::tuple<int, char>> expected{
      {1, 'x'},
      {2, 'y'},
      {3, 'z'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, ListAndDeque)
{
    std::list<int>   lst{1, 2, 3};
    std::deque<char> dq{'a', 'b', 'c'};

    auto z = radr::zip(std::ref(lst), std::ref(dq));

    std::vector<std::tuple<int, char>> expected{
      {1, 'a'},
      {2, 'b'},
      {3, 'c'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, BorrowedRanges)
{
    std::vector<int> vec{1, 2, 3};
    std::string_view sv = "abc";

    auto z = radr::zip(std::ref(vec), std::ref(sv));

    std::vector<std::tuple<int, char>> expected{
      {1, 'a'},
      {2, 'b'},
      {3, 'c'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

TEST(zip, WithRadrRanges)
{
    auto              iota_rng = radr::iota(0, 3);
    std::vector<char> chars{'a', 'b', 'c'};

    auto z = radr::zip(std::move(iota_rng), std::ref(chars));

    std::vector<std::tuple<int, char>> expected{
      {0, 'a'},
      {1, 'b'},
      {2, 'c'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
}

// ---------- Concept checks ----------

TEST(zip, ConceptsForRandomAccessRanges)
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{4, 5, 6};
    auto             z = radr::zip(std::ref(a), std::ref(b));

    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
    EXPECT_TRUE(std::ranges::input_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::forward_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::random_access_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::common_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(z)>);
}

TEST(zip, ConceptsForBidirectionalRanges)
{
    std::list<int> a{1, 2, 3};
    std::list<int> b{4, 5, 6};
    auto           z = radr::zip(std::ref(a), std::ref(b));

    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
    EXPECT_TRUE(std::ranges::input_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::forward_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(z)>);
}

TEST(zip, ConceptsForForwardRanges)
{
    std::forward_list<int> a{1, 2, 3};
    std::forward_list<int> b{4, 5, 6};
    auto                   z = radr::zip(std::ref(a), std::ref(b));

    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
    EXPECT_TRUE(std::ranges::input_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::forward_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::common_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(z)>);
}

TEST(zip, ConceptsMixedCategories)
{
    std::vector<int> vec{1, 2, 3};
    std::list<int>   lst{4, 5, 6};
    auto             z = radr::zip(std::ref(vec), std::ref(lst));

    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::borrowed_range<decltype(z)>);
}

// ---------- Concept checks ----------

TEST(zip, ConceptsForRandomAccessRangesOwning)
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{4, 5, 6};
    auto             z = radr::zip(std::move(a), std::move(b));

    EXPECT_EQ(check_rad_type(z), rad_type::zip_rng);
    EXPECT_TRUE(std::ranges::input_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::forward_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::random_access_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::common_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::borrowed_range<decltype(z)>);
}

TEST(zip, ConceptsForBidirectionalRangesOwning)
{
    std::list<int> a{1, 2, 3};
    std::list<int> b{4, 5, 6};
    auto           z = radr::zip(std::move(a), std::move(b));

    EXPECT_EQ(check_rad_type(z), rad_type::zip_rng);
    EXPECT_TRUE(std::ranges::input_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::forward_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::borrowed_range<decltype(z)>);
}

TEST(zip, ConceptsForForwardRangesOwning)
{
    std::forward_list<int> a{1, 2, 3};
    std::forward_list<int> b{4, 5, 6};
    auto                   z = radr::zip(std::move(a), std::move(b));

    EXPECT_EQ(check_rad_type(z), rad_type::zip_rng);
    EXPECT_TRUE(std::ranges::input_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::forward_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::common_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::borrowed_range<decltype(z)>);
}

TEST(zip, ConceptsMixedCategoriesOwning)
{
    std::vector<int> vec{1, 2, 3};
    std::list<int>   lst{4, 5, 6};
    auto             z = radr::zip(std::move(vec), std::move(lst));

    EXPECT_EQ(check_rad_type(z), rad_type::zip_rng);
    EXPECT_TRUE(std::ranges::bidirectional_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(z)>);
    EXPECT_TRUE(std::ranges::sized_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::borrowed_range<decltype(z)>);
}

// ---------- Iterator operations ----------

TEST(zip, RandomAccessIndexing)
{
    std::vector<int>  a{1, 2, 3, 4, 5};
    std::vector<char> b{'a', 'b', 'c', 'd', 'e'};
    auto              z = radr::zip(std::ref(a), std::ref(b));

    auto it = z.begin();
    EXPECT_EQ(std::get<0>(it[0]), 1);
    EXPECT_EQ(std::get<1>(it[0]), 'a');
    EXPECT_EQ(std::get<0>(it[2]), 3);
    EXPECT_EQ(std::get<1>(it[2]), 'c');
    EXPECT_EQ(std::get<0>(it[4]), 5);
    EXPECT_EQ(std::get<1>(it[4]), 'e');
}

TEST(zip, IteratorDistance)
{
    std::vector<int> a{1, 2, 3, 4};
    std::vector<int> b{10, 20, 30, 40};
    auto             z = radr::zip(std::ref(a), std::ref(b));

    auto first = z.begin();
    auto last  = z.end();
    EXPECT_EQ(last - first, 4);
}

TEST(zip, IteratorAdvance)
{
    std::vector<int> a{1, 2, 3, 4, 5};
    std::vector<int> b{10, 20, 30, 40, 50};
    auto             z = radr::zip(std::ref(a), std::ref(b));

    auto it = z.begin();
    it += 2;
    auto [x, y] = *it;
    EXPECT_EQ(x, 3);
    EXPECT_EQ(y, 30);

    it -= 1;
    auto [x2, y2] = *it;
    EXPECT_EQ(x2, 2);
    EXPECT_EQ(y2, 20);
}

TEST(zip, IteratorIncrement)
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{10, 20, 30};
    auto             z = radr::zip(std::ref(a), std::ref(b));

    auto it = z.begin();
    EXPECT_EQ(std::get<0>(*it), 1);

    ++it;
    EXPECT_EQ(std::get<0>(*it), 2);

    it++;
    EXPECT_EQ(std::get<0>(*it), 3);

    ++it;
    EXPECT_EQ(it, z.end());
}

TEST(zip, IteratorComparison)
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{10, 20, 30};
    auto             z = radr::zip(std::ref(a), std::ref(b));

    auto it1 = z.begin();
    auto it2 = z.begin();
    auto it3 = z.begin() + 1;

    EXPECT_EQ(it1, it2);
    EXPECT_NE(it1, it3);
    EXPECT_LT(it1, it3);
    EXPECT_LE(it1, it3);
    EXPECT_GT(it3, it1);
    EXPECT_GE(it3, it1);
}

// ---------- Size and common range ----------

TEST(zip, SizeIsMinimum)
{
    std::vector<int> a{1, 2, 3, 4, 5};
    std::vector<int> b{10, 20};
    std::vector<int> c{100, 200, 300, 400};
    auto             z = radr::zip(std::ref(a), std::ref(b), std::ref(c));

    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
    EXPECT_EQ(std::ranges::size(z), 2u);
}

TEST(zip, CommonRangeWithCommonInputs)
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{4, 5, 6};
    auto             z = radr::zip(std::ref(a), std::ref(b));

    EXPECT_EQ(check_rad_type(z), rad_type::borrowing_rad);
    EXPECT_TRUE(std::ranges::common_range<decltype(z)>);
}

// ---------- Copyability and ownership ----------

TEST(zip, owning_copy_test)
{
    std::vector<std::tuple<int, int>> const comp{
      {1, 0},
      {2, 1},
      {3, 2},
      {4, 3},
      {5, 4},
      {6, 5}
    };
    using T = decltype(radr::zip(std::vector{1, 2, 3, 4, 5, 6}, radr::iota(0)));

    T cpy;

    {
        T own = radr::zip(std::vector{1, 2, 3, 4, 5, 6}, radr::iota(0));
        EXPECT_EQ(check_rad_type(own), rad_type::zip_rng);
        EXPECT_RANGE_EQ(own, comp);
        cpy = own;
    }

    EXPECT_RANGE_EQ(cpy, comp);
}

TEST(zip, BorrowingZipIsCopyable)
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{4, 5, 6};
    auto             z1 = radr::zip(std::ref(a), std::ref(b));
    auto             z2 = z1;

    EXPECT_EQ(check_rad_type(z1), rad_type::borrowing_rad);
    EXPECT_EQ(check_rad_type(z2), rad_type::borrowing_rad);
    std::vector<std::tuple<int, int>> expected{
      {1, 4},
      {2, 5},
      {3, 6}
    };
    EXPECT_RANGE_EQ(z1, expected);
    EXPECT_RANGE_EQ(z2, expected);
}

//===========================================================================
// radr::zip_sp - Single-pass version
//===========================================================================

TEST(zip_sp, SingleRange)
{
    std::vector<int> vec{1, 2, 3};
    auto             z = radr::zip_sp(std::move(vec));

    std::vector<std::tuple<int>> expected{{1}, {2}, {3}};
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::generator);
}

TEST(zip_sp, TwoRanges)
{
    std::vector<int>  a{1, 2, 3};
    std::vector<char> b{'a', 'b', 'c'};
    auto              z = radr::zip_sp(std::move(a), std::move(b));

    std::vector<std::tuple<int, char>> expected{
      {1, 'a'},
      {2, 'b'},
      {3, 'c'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::generator);
}

TEST(zip_sp, ThreeRanges)
{
    std::vector<int>    a{1, 2, 3};
    std::vector<char>   b{'x', 'y', 'z'};
    std::vector<double> c{1.1, 2.2, 3.3};
    auto                z = radr::zip_sp(std::move(a), std::move(b), std::move(c));

    std::vector<std::tuple<int, char, double>> expected{
      {1, 'x', 1.1},
      {2, 'y', 2.2},
      {3, 'z', 3.3}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::generator);
}

TEST(zip_sp, EmptyRanges)
{
    std::vector<int> a{};
    std::vector<int> b{};
    auto             z = radr::zip_sp(std::move(a), std::move(b));

    EXPECT_TRUE(z.begin() == z.end());
    EXPECT_EQ(check_rad_type(z), rad_type::generator);
}

TEST(zip_sp, DifferentLengths)
{
    std::vector<int>  a{1, 2, 3, 4, 5};
    std::vector<char> b{'a', 'b'};
    auto              z = radr::zip_sp(std::move(a), std::move(b));

    std::vector<std::tuple<int, char>> expected{
      {1, 'a'},
      {2, 'b'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::generator);
}

TEST(zip_sp, MixedSinglePassAndMultiPass)
{
    auto              iota_gen = radr::iota_sp(0, 3);
    std::vector<char> chars{'a', 'b', 'c'};
    auto              z = radr::zip_sp(std::move(iota_gen), std::move(chars));

    std::vector<std::tuple<int, char>> expected{
      {0, 'a'},
      {1, 'b'},
      {2, 'c'}
    };
    EXPECT_RANGE_EQ(z, expected);
    EXPECT_EQ(check_rad_type(z), rad_type::generator);
}

TEST(zip_sp, ConceptChecks)
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{4, 5, 6};
    auto             z = radr::zip_sp(std::move(a), std::move(b));

    EXPECT_EQ(check_rad_type(z), rad_type::generator);
    EXPECT_TRUE(std::ranges::input_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::forward_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::bidirectional_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::random_access_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::sized_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::common_range<decltype(z)>);
    EXPECT_FALSE(std::ranges::borrowed_range<decltype(z)>);
}

TEST(zip_sp, ReferenceTypes)
{
    std::vector<int>         xs{1, 2, 3};
    std::vector<std::string> ys{"a", "b", "c"};
    auto                     z = radr::zip_sp(std::move(xs), std::move(ys));

    EXPECT_EQ(check_rad_type(z), rad_type::generator);
    using Ref = std::ranges::range_reference_t<decltype(z)>;
    using Val = std::ranges::range_value_t<decltype(z)>;

    EXPECT_SAME_TYPE(Ref, (std::tuple<int &, std::string &>));
    EXPECT_SAME_TYPE(Val, (std::tuple<int, std::string>));
}

TEST(zip_sp, ReferenceTypes2)
{
    std::vector<int> xs{1, 2, 3};
    auto             z = radr::zip_sp(std::move(xs), radr::iota(0));

    EXPECT_EQ(check_rad_type(z), rad_type::generator);
    using Ref = std::ranges::range_reference_t<decltype(z)>;
    using Val = std::ranges::range_value_t<decltype(z)>;

    EXPECT_SAME_TYPE(Ref, (std::tuple<int &, int>));
    EXPECT_SAME_TYPE(Val, (std::tuple<int, int>));
}

TEST(zip_sp, Iteration)
{
    std::vector<int> a{1, 2, 3, 4};
    std::vector<int> b{10, 20, 30, 40};
    auto             z = radr::zip_sp(std::move(a), std::move(b));

    auto it = z.begin();
    auto e  = z.end();

    ASSERT_NE(it, e);
    EXPECT_EQ(*it, (std::tuple{1, 10}));

    ++it;
    ASSERT_NE(it, e);
    EXPECT_EQ(*it, (std::tuple{2, 20}));

    ++it;
    ASSERT_NE(it, e);
    EXPECT_EQ(*it, (std::tuple{3, 30}));

    ++it;
    ASSERT_NE(it, e);
    EXPECT_EQ(*it, (std::tuple{4, 40}));

    ++it;
    EXPECT_EQ(it, e);
}

TEST(zip_sp, StopsAtShortest)
{
    std::vector<int>  a{1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<char> b{'a', 'b', 'c'};
    auto              z = radr::zip_sp(std::move(a), std::move(b));

    int count = 0;
    for ([[maybe_unused]] auto _ : z)
    {
        count++;
        EXPECT_LE(count, 3);
    }
    EXPECT_EQ(count, 3);
}

TEST(zip_sp, FourRanges)
{
    std::vector<int>    a{1, 2};
    std::vector<char>   b{'a', 'b'};
    std::vector<double> c{1.1, 2.2};
    std::vector<bool>   d{true, false};
    auto                z = radr::zip_sp(std::move(a), std::move(b), std::move(c), std::move(d));

    EXPECT_EQ(check_rad_type(z), rad_type::generator);
    std::vector<std::tuple<int, char, double, bool>> expected{
      {1, 'a', 1.1,  true},
      {2, 'b', 2.2, false}
    };
    EXPECT_RANGE_EQ(z, expected);
}

#endif
