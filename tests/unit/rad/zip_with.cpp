// zip_view_gtest.cpp
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <list>
#include <memory>
#include <numeric>
#include <ranges>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <radr/rad/zip_with.hpp>


// ---------- Basic behavior & edge cases ----------

TEST(ZipView, BasicZipsToShortestWithCorrectValues) {
  std::vector<int> a{1, 2, 3, 4};
  std::vector<char> b{'a', 'b', 'c'};
  auto z = radr::zip_with(std::ref(a), std::ref(b));

  static_assert(std::ranges::sized_range<decltype(z)>);
  EXPECT_EQ(std::ranges::size(z), 3u);

  std::vector<std::pair<int, char>> got;
  for (auto [i, ch] : z) {
    got.emplace_back(i, ch);
  }
  ASSERT_EQ(got.size(), 3u);
  EXPECT_EQ(got[0], (std::pair{1, 'a'}));
  EXPECT_EQ(got[1], (std::pair{2, 'b'}));
  EXPECT_EQ(got[2], (std::pair{3, 'c'}));
}

TEST(ZipView, WorksWithEmptyRanges) {
  std::vector<int> a{};
  std::vector<int> b{1, 2, 3};
  auto z1 = radr::zip_with(std::ref(a), std::ref(b));
  auto z2 = radr::zip_with(std::ref(a), std::ref(a))
  auto z3 = std::views::zip(b, std::vector<int>{});

  static_assert(std::ranges::sized_range<decltype(z1)>);
  EXPECT_EQ(std::ranges::size(z1), 0u);
  EXPECT_TRUE(radr::begin(z1) == radr::end(z1));

  EXPECT_EQ(std::ranges::size(z2), 0u);
  EXPECT_TRUE(radr::begin(z2) == radr::end(z2));

  EXPECT_EQ(std::ranges::size(z3), 0u);
  EXPECT_TRUE(radr::begin(z3) == radr::end(z3));
}

TEST(ZipView, TruncatesWhenLengthsDiffer) {
  std::array<int, 5> a{1, 2, 3, 4, 5};
  std::array<int, 2> b{10, 20};
  auto z = radr::zip_with(std::ref(a), std::ref(b));
  static_assert(std::ranges::sized_range<decltype(z)>);
  ASSERT_EQ(std::ranges::size(z), 2u);

  auto it = radr::begin(z);
  ASSERT_NE(it, radr::end(z));
  EXPECT_EQ(std::get<0>(*it), 1);
  EXPECT_EQ(std::get<1>(*it), 10);

  ++it;
  ASSERT_NE(it, radr::end(z));
  EXPECT_EQ(std::get<0>(*it), 2);
  EXPECT_EQ(std::get<1>(*it), 20);

  ++it;
  EXPECT_EQ(it, radr::end(z));
}

TEST(ZipView, StructuredBindingsYieldReferencesAndAreWritable) {
  std::vector<int> xs{1, 2, 3};
  std::vector<int> ys{10, 20, 30};

  for (auto [x, y] : std::views::zip(xs, ys)) {
    x += y;
  }
  EXPECT_EQ(xs, (std::vector<int>{11, 22, 33}));
}

TEST(ZipView, ConstIteratesButDoesNotPermitMutation) {
  const std::vector<int> xs{1, 2, 3};
  const std::vector<int> ys{4, 5, 6};

  auto z = std::views::zip(xs, ys);
  static_assert(std::is_same_v<std::ranges::range_reference_t<decltype(z)>,
                               std::tuple<const int&, const int&>>);

  int sum = 0;
  for (auto [a, b] : z) {
    sum += a + b;
  }
  EXPECT_EQ(sum, (1 + 4) + (2 + 5) + (3 + 6));
}

TEST(ZipView, SupportsMoveOnlyElementsViaReference) {
  std::vector<std::unique_ptr<int>> ptrs;
  ptrs.emplace_back(std::make_unique<int>(1));
  ptrs.emplace_back(std::make_unique<int>(2));
  std::vector<int> mult{10, 20};

  auto z = std::views::zip(ptrs, mult);
  static_assert(std::is_same_v<std::ranges::range_reference_t<decltype(z)>,
                               std::tuple<std::unique_ptr<int>&, int&>>);

  for (auto [p, m] : z) {
    *p *= m;
  }
  ASSERT_EQ(*ptrs[0], 10);
  ASSERT_EQ(*ptrs[1], 40);
}

// ---------- Concept propagation & category tests ----------

TEST(ZipView, IsAViewAndAnInputRange) {
  std::vector<int> a{1, 2};
  std::vector<int> b{3, 4};
  auto z = radr::zip_with(std::ref(a), std::ref(b));

  static_assert(std::ranges::view<decltype(z)>);
  static_assert(std::ranges::input_range<decltype(z)>);
  static_assert(std::ranges::common_range<decltype(z)>);
  static_assert(std::ranges::forward_range<decltype(z)>);
  static_assert(std::ranges::random_access_range<decltype(z)>);
  static_assert(std::ranges::sized_range<decltype(z)>);
}

TEST(ZipView, RandomAccessAndSizedOnlyIfAllConstituentsAre) {
  std::vector<int> v{1, 2, 3};
  std::list<int> lst{10, 20, 30};

  auto z1 = std::views::zip(v, v);
  auto z2 = std::views::zip(v, lst);
  auto z3 = std::views::zip(lst, lst);
  auto z4 = std::views::zip(v, std::views::iota(0, 3));

  static_assert(std::ranges::random_access_range<decltype(z1)>);
  static_assert(std::ranges::sized_range<decltype(z1)>);
  static_assert(std::ranges::common_range<decltype(z1)>);

  static_assert(!std::ranges::random_access_range<decltype(z2)>);
  static_assert(std::ranges::bidirectional_range<decltype(z2)>);
  static_assert(!std::ranges::common_range<decltype(z2)>);
  static_assert(std::ranges::sized_range<decltype(z2)>);

  static_assert(!std::ranges::random_access_range<decltype(z3)>);
  static_assert(std::ranges::bidirectional_range<decltype(z3)>);
  static_assert(!std::ranges::common_range<decltype(z3)>);
  static_assert(std::ranges::sized_range<decltype(z3)>);

  static_assert(std::ranges::random_access_range<decltype(z4)>);
  static_assert(std::ranges::sized_range<decltype(z4)>);
  static_assert(std::ranges::common_range<decltype(z4)>);
}

TEST(ZipView, ForwardOnlyIfAllAreAtLeastForward) {
  struct InputOnly {
    int n;
    struct iterator {
      using value_type = int;
      using difference_type = std::ptrdiff_t;
      using iterator_concept = std::input_iterator_tag;
      int* cur;
      int remaining;
      int operator*() const { return *cur; }
      iterator& operator++() { --remaining; ++cur; return *this; }
      void operator++(int) { ++(*this); }
      bool operator==(std::default_sentinel_t) const { return remaining == 0; }
    };
    iterator begin() { return iterator{&n, 1}; }
    std::default_sentinel_t end() { return {}; }
  };

  InputOnly in{42};
  std::vector<int> vec{1, 2, 3};

  auto z = std::views::zip(in, vec);
  static_assert(std::ranges::input_range<decltype(z)>);
  static_assert(!std::ranges::forward_range<decltype(z)>);
}

TEST(ZipView, CommonRangeDependsOnConstituents) {
  auto non_common = std::views::iota(0) | std::views::take_while([](int x){ return x < 3; });
  std::vector<int> v{7,8,9};

  auto z1 = std::views::zip(v, v);
  auto z2 = std::views::zip(non_common, v);

  static_assert(std::ranges::common_range<decltype(z1)>);
  static_assert(!std::ranges::common_range<decltype(z2)>);
}

TEST(ZipView, BorrowedRangeOnlyIfAllAreBorrowed) {
  std::array<int, 3> a{1,2,3};
  std::array<int, 3> b{4,5,6};
  std::vector<int> v{7,8,9};

  auto z_arr = radr::zip_with(std::ref(a), std::ref(b));
  auto z_mixed = std::views::zip(a, v);

  static_assert(std::ranges::borrowed_range<decltype(z_arr)>);
  static_assert(std::ranges::borrowed_range<decltype(z_mixed)>);
}

// ---------- Iterator semantics & tuple/reference types ----------

TEST(ZipView, ReferenceTypesAreTuplesOfReferences) {
  std::vector<int> xs{1,2};
  std::vector<std::string> ys{"a","b"};

  auto z = std::views::zip(xs, ys);
  using Ref = std::ranges::range_reference_t<decltype(z)>;
  static_assert(std::is_same_v<Ref, std::tuple<int&, std::string&>>);

  auto it = radr::begin(z);
  auto [i0, s0] = *it;
  EXPECT_EQ(i0, 1);
  EXPECT_EQ(s0, "a");
  i0 = 10;
  s0 = "A";
  EXPECT_EQ(xs[0], 10);
  EXPECT_EQ(ys[0], "A");
}

TEST(ZipView, RandomAccessIndexingAndDistance) {
  std::array<int, 4> a{1,2,3,4};
  std::array<int, 4> b{10,20,30,40};
  auto z = radr::zip_with(std::ref(a), std::ref(b));
  static_assert(std::ranges::random_access_range<decltype(z)>);

  auto it = radr::begin(z);
  EXPECT_EQ(std::get<0>(it[2]), 3);
  EXPECT_EQ(std::get<1>(it[2]), 30);

  auto first = radr::begin(z);
  auto last  = radr::end(z);
  EXPECT_EQ(last - first, 4);
}

TEST(ZipView, SizeIsMinimumOfConstituentSizes) {
  std::array<int, 5> a{1,2,3,4,5};
  std::vector<int> b{9,8,7};
  std::vector<int> c{100,200,300,400};
  auto z = std::views::zip(a, b, c);
  static_assert(std::ranges::sized_range<decltype(z)>);
  EXPECT_EQ(std::ranges::size(z), 3u);
}

// ---------- Algorithms integration ----------

TEST(ZipView, WorksWithRangesAlgorithms) {
  std::vector<int> x{1,2,3};
  std::vector<int> y{4,5,6};
  int sum = 0;
  for (auto [a, b] : std::views::zip(x, y)) {
    sum += a * b;
  }
  EXPECT_EQ(sum, 1*4 + 2*5 + 3*6);
}

TEST(ZipView, MutationThroughAlgorithms) {
  std::vector<int> x{1,2,3};
  std::vector<int> y{10,20,30};
  for (auto t : std::views::zip(x, y)) {
    std::get<0>(t) += std::get<1>(t);
  }
  EXPECT_EQ(x, (std::vector<int>{11,22,33}));
}

// ---------- Const/temporary lifetime safety ----------

TEST(ZipView, ZipOfTemporariesLivesAsView) {
  auto z = std::views::zip(std::vector<int>{1,2}, std::array<int,2>{3,4});
  static_assert(std::ranges::view<decltype(z)>);

  int total = 0;
  for (auto [a,b] : z) total += a + b;
  EXPECT_EQ(total, 1+3 + 2+4);
}

