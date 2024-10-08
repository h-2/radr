#include <forward_list>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/concepts.hpp>
#include <radr/rad/drop.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/join.hpp>
#include <radr/rad/take.hpp>
#include <radr/rad/transform.hpp>

TEST(iterator_size, transform)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto plus2 = [](int i)
    {
        return i + 2;
    };
    auto plus3 = [](int i)
    {
        return i + 3;
    };
    auto plus4 = [](int i)
    {
        return i + 4;
    };

    std::vector<int> vec{};

    {
        auto v = vec | std::views::transform(plus1) | std::views::transform(plus2) | std::views::transform(plus3) |
                 std::views::transform(plus4);
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE < 12)
        EXPECT_EQ(sizeof(v), 40);
#else
        EXPECT_EQ(sizeof(v), 8);
#endif
        EXPECT_EQ(sizeof(v.begin()), 40);
        EXPECT_EQ(sizeof(v.end()), 40);
    }

    {
        auto r = std::ref(vec) | radr::transform(plus1) | radr::transform(plus2) | radr::transform(plus3) |
                 radr::transform(plus4);
        EXPECT_EQ(sizeof(r), 16);
        EXPECT_EQ(sizeof(r.begin()), 8);
        EXPECT_EQ(sizeof(r.end()), 8);
    }
}

TEST(iterator_size, transform_big)
{
    auto plus1 = [s = std::string{"foobar"}](int i)
    {
        return i + 1;
    };
    auto plus2 = [s = std::string{"foobar"}](int i)
    {
        return i + 2;
    };
    auto plus3 = [s = std::string{"foobar"}](int i)
    {
        return i + 3;
    };
    auto plus4 = [s = std::string{"foobar"}](int i)
    {
        return i + 4;
    };

    std::vector<int> vec{};

    {
        auto v = vec | std::views::transform(plus1) | std::views::transform(plus2) | std::views::transform(plus3) |
                 std::views::transform(plus4);
#ifdef _LIBCPP_RANGES
        EXPECT_EQ(sizeof(v), 136);
#else
        EXPECT_EQ(sizeof(v), 168);
#endif
        EXPECT_EQ(sizeof(v.begin()), 40);
        EXPECT_EQ(sizeof(v.end()), 40);
    }

    {
        auto r = std::ref(vec) | radr::transform(plus1) | radr::transform(plus2) | radr::transform(plus3) |
                 radr::transform(plus4);
#ifdef _LIBCPP_RANGES
        EXPECT_EQ(sizeof(r), 224);
        EXPECT_EQ(sizeof(r.begin()), 112);
        EXPECT_EQ(sizeof(r.end()), 112);
#else
        EXPECT_EQ(sizeof(r), 288);
        EXPECT_EQ(sizeof(r.begin()), 144);
        EXPECT_EQ(sizeof(r.end()), 144);
#endif
    }
}

TEST(iterator_size, transform_big_ref)
{
    auto plus1 = [s = std::string{"foobar"}](int i)
    {
        return i + 1;
    };
    auto plus2 = [s = std::string{"foobar"}](int i)
    {
        return i + 2;
    };
    auto plus3 = [s = std::string{"foobar"}](int i)
    {
        return i + 3;
    };
    auto plus4 = [s = std::string{"foobar"}](int i)
    {
        return i + 4;
    };

    std::vector<int> vec{};

    {
        auto v = vec | std::views::transform(std::ref(plus1)) | std::views::transform(std::ref(plus2)) |
                 std::views::transform(std::ref(plus3)) | std::views::transform(std::ref(plus4));
        EXPECT_EQ(sizeof(v), 40);
        EXPECT_EQ(sizeof(v.begin()), 40);
        EXPECT_EQ(sizeof(v.end()), 40);
    }

    {
        auto r = std::ref(vec) | radr::transform(std::ref(plus1)) | radr::transform(std::ref(plus2)) |
                 radr::transform(std::ref(plus3)) | radr::transform(std::ref(plus4));
        EXPECT_EQ(sizeof(r), 96);
        EXPECT_EQ(sizeof(r.begin()), 48);
        EXPECT_EQ(sizeof(r.end()), 48);
    }
}

TEST(iterator_size, filter)
{
    auto mod1 = [](int i)
    {
        return i % 1 == 0;
    };
    auto mod2 = [](int i)
    {
        return i % 2 == 0;
    };
    auto mod3 = [](int i)
    {
        return i % 3 == 0;
    };
    auto mod4 = [](int i)
    {
        return i % 4 == 0;
    };

    std::vector<int> vec{};

    {
        auto v = vec | std::views::filter(mod1) | std::views::filter(mod2) | std::views::filter(mod3) |
                 std::views::filter(mod4);
#ifdef _LIBCPP_RANGES
        EXPECT_EQ(sizeof(v), 120);
#else
        EXPECT_EQ(sizeof(v), 112);
#endif
        EXPECT_EQ(sizeof(v.begin()), 40);
        EXPECT_EQ(sizeof(v.end()), 40);
    }

    {
        auto r = std::ref(vec) | radr::filter(mod1) | radr::filter(mod2) | radr::filter(mod3) | radr::filter(mod4);
        EXPECT_EQ(sizeof(r), 16);
        EXPECT_EQ(sizeof(r.begin()), 16);
        EXPECT_EQ(sizeof(r.end()), 1);
    }
}

TEST(iterator_size, filter_transform)
{
    auto plus1 = [](int i)
    {
        return i + 1;
    };
    auto plus2 = [](int i)
    {
        return i + 2;
    };
    auto mod1 = [](int i)
    {
        return i % 1 == 0;
    };
    auto mod2 = [](int i)
    {
        return i % 2 == 0;
    };

    std::vector<int> vec{};

    {
        auto v = vec | std::views::transform(plus1) | std::views::filter(mod1) | std::views::transform(plus2) |
                 std::views::filter(mod2);
#if defined(_LIBCPP_RANGES) || (defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE < 12))
        EXPECT_EQ(sizeof(v), 72);
#else
        EXPECT_EQ(sizeof(v), 56);
#endif
        EXPECT_EQ(sizeof(v.begin()), 40);
        EXPECT_EQ(sizeof(v.end()), 40);
    }

    {
        auto r =
          std::ref(vec) | radr::transform(plus1) | radr::filter(mod1) | radr::transform(plus2) | radr::filter(mod2);
        EXPECT_EQ(sizeof(r), 24);
        EXPECT_EQ(sizeof(r.begin()), 16);
        EXPECT_EQ(sizeof(r.end()), 1);
    }
}

TEST(iterator_size, take_drop_contig)
{
    std::vector<int> l{};

    {
        auto v = l | std::views::take(1) | std::views::drop(1) | std::views::take(1) | std::views::drop(1);
#if defined(_LIBCPP_VERSION) || (defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE < 12))
        EXPECT_EQ(sizeof(v), 40);
#else
        EXPECT_EQ(sizeof(v), 48);
#endif
        EXPECT_EQ(sizeof(v.begin()), 8);
        EXPECT_EQ(sizeof(v.end()), 8);
    }

    {
        auto v = std::ref(l) | radr::take(1) | radr::drop(1) | radr::take(1) | radr::drop(1);
        EXPECT_EQ(sizeof(v), 16);
        EXPECT_EQ(sizeof(v.begin()), 8);
        EXPECT_EQ(sizeof(v.end()), 8);
    }
}

TEST(iterator_size, take_drop_bidi)
{
    std::list<int> l{};

    {
        auto v = l | std::views::take(1) | std::views::drop(1) | std::views::take(1) | std::views::drop(1);
        EXPECT_EQ(sizeof(v), 96);
        EXPECT_EQ(sizeof(v.begin()), 24);
        EXPECT_EQ(sizeof(v.end()), 1);
    }

    {
        auto v = std::ref(l) | radr::take(1) | radr::drop(1) | radr::take(1) | radr::drop(1);
        EXPECT_EQ(sizeof(v), 16);
        EXPECT_EQ(sizeof(v.begin()), 16);
        EXPECT_EQ(sizeof(v.end()), 1);
    }
}

TEST(iterator_size, join_fwd)
{
    std::forward_list<std::string> l;
    EXPECT_EQ(sizeof(l.begin()), 8);

#if !(defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION < 180000)) // doesn't have views::join
    {
        auto v = l | std::views::join;

#    if defined(_LIBCPP_VERSION) || (defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE <= 11))
        EXPECT_EQ(sizeof(v), 16);
#    else
        EXPECT_EQ(sizeof(v), 8);
#    endif

#    if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE <= 12)
        EXPECT_EQ(sizeof(v.begin()), 24);
        EXPECT_EQ(sizeof(v.end()), 24);
#    else
        EXPECT_EQ(sizeof(v.begin()), 32);
        EXPECT_EQ(sizeof(v.end()), 32);
#    endif
    }
#endif

    {
        auto v = std::ref(l) | radr::join;
        EXPECT_EQ(sizeof(v), 64);
        EXPECT_EQ(sizeof(v.begin()), 32);
        EXPECT_EQ(sizeof(v.end()), 32);
    }
}

TEST(iterator_size, join_bidi)
{
    std::vector<std::string> l;

    // std version same as above

    {
        auto v = std::ref(l) | radr::join;
        EXPECT_EQ(sizeof(v), 96);
        EXPECT_EQ(sizeof(v.begin()), 48);
        EXPECT_EQ(sizeof(v.end()), 48);
    }
}
