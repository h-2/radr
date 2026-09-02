#include <deque>
#include <forward_list>
#include <functional>
#include <list>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>
#include <radr/test/adaptor_template.hpp>
#include <radr/test/aux_ranges.hpp>
#include <radr/test/gtest_helpers.hpp>

#include <radr/concepts.hpp>
#include <radr/rad/chunk.hpp>
#include <radr/rad/chunk_by.hpp>
#include <radr/rad/drop.hpp>
#include <radr/rad/filter.hpp>
#include <radr/rad/join.hpp>
#include <radr/rad/lazy_chunk.hpp>
#include <radr/rad/take.hpp>
#include <radr/rad/take_while.hpp>
#include <radr/rad/transform.hpp>
#include <radr/version.hpp>

#if RADR_FEATURE_ZIP
#    include <radr/rad/adjacent.hpp>
#endif

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
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE == 12 || _GLIBCXX_RELEASE == 13)
        EXPECT_EQ(sizeof(v), 48);
#else
        EXPECT_EQ(sizeof(v), 40);
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

TEST(iterator_size, chunk_by_unidi)
{
    std::forward_list<int> vec{};

#ifdef __cpp_lib_ranges_chunk_by
    {
        auto v = vec | std::views::chunk_by(std::equal_to<>{});
#    ifdef _LIBCPP_VERSION
        EXPECT_EQ(sizeof(v), 24); // libc++ does not pad the view
#    else
        EXPECT_EQ(sizeof(v), 32);
#    endif
        EXPECT_EQ(sizeof(v.begin()), 24);
        EXPECT_EQ(sizeof(v.end()), 24);
    }
#endif

    auto v = std::ref(vec) | radr::chunk_by(std::equal_to<>{});
    EXPECT_EQ(sizeof(v), 24);
    EXPECT_EQ(sizeof(v.begin()), 24);
    EXPECT_EQ(sizeof(v.end()), 1); // un-common, so we save empty sentinel
}

TEST(iterator_size, chunk_by_bidi)
{
    std::vector<int> vec{};

#ifdef __cpp_lib_ranges_chunk_by
    {
        // std::views::chunk_by has no size/capabilities split, and no separate random-access
        // implementation either (its boundaries are content-dependent, same reasoning as
        // radr::chunk_by never getting one) -- these are the same numbers as the chunk_by test above.
        auto v = vec | std::views::chunk_by(std::equal_to<>{});
        EXPECT_EQ(sizeof(v), 24);
        EXPECT_EQ(sizeof(v.begin()), 24);
        EXPECT_EQ(sizeof(v.end()), 24);
    }
#endif

    // common, so begin and end are the same type and the range is just the two of them
    auto v = std::ref(vec) | radr::chunk_by(std::equal_to<>{});
    EXPECT_EQ(sizeof(v), 64);
    EXPECT_EQ(sizeof(v.begin()), 32);
    EXPECT_EQ(sizeof(v.end()), 32);
}

TEST(iterator_size, chunk_unidi)
{
    std::forward_list<int> vec{};

#ifdef __cpp_lib_ranges_chunk
    {
        auto v = vec | std::views::chunk(2);
        EXPECT_EQ(sizeof(v), 16);
        EXPECT_EQ(sizeof(v.begin()), 32);
        EXPECT_EQ(sizeof(v.end()), 32);
    }
#endif

    auto v = std::ref(vec) | radr::chunk(2);
    EXPECT_EQ(sizeof(v), 32);
    EXPECT_EQ(sizeof(v.begin()), 32);
    EXPECT_EQ(sizeof(v.end()), 1); // un-common, so we save empty sentinel
}

TEST(iterator_size, chunk_bidi_common)
{
    std::list<int> l{};

#ifdef __cpp_lib_ranges_chunk
    {
        // std::views::chunk has no size/capabilities split -- this is the same view template as in
        // the chunk test above, just with a bidirectional, non-random-access base.
        auto v = l | std::views::chunk(2);
        EXPECT_EQ(sizeof(v), 16);
        EXPECT_EQ(sizeof(v.begin()), 32);
        EXPECT_EQ(sizeof(v.end()), 32);
    }
#endif

    // common, so begin and end are the same type; the range is the two of them plus 8 for the size
    auto v = std::ref(l) | radr::chunk(2);
    EXPECT_EQ(sizeof(v), 88);
    EXPECT_EQ(sizeof(v.begin()), 40);
    EXPECT_EQ(sizeof(v.end()), 40);
}

// ra_chunk_like_iterator stores one underlying iterator plus three cheap integers, instead of
// bidi_chunk_like_iterator's two-or-more full (and for std::deque specifically, "fat") iterators.
TEST(iterator_size, chunk_ra_sized)
{
    std::vector<int> vec{};
    std::deque<int>  deq{};

#ifdef __cpp_lib_ranges_chunk
    {
        // std::views::chunk's outer iterator doesn't get any smaller for a random-access base either
        // -- it always caches begin/end/n unconditionally, unlike radr::chunk's dedicated
        // ra_chunk_like_iterator, which is exactly the point of the latter's existence.
        auto v = vec | std::views::chunk(2);
        EXPECT_EQ(sizeof(v), 16);
        EXPECT_EQ(sizeof(v.begin()), 32);
        EXPECT_EQ(sizeof(v.end()), 32);

        auto d = deq | std::views::chunk(2);
        EXPECT_EQ(sizeof(d), 16);
        EXPECT_EQ(sizeof(d.begin()), 80);
        EXPECT_EQ(sizeof(d.end()), 80);
    }
#endif

    auto v = std::ref(vec) | radr::chunk(2);
    EXPECT_EQ(sizeof(v), 64);
    EXPECT_EQ(sizeof(v.begin()), 32);
    EXPECT_EQ(sizeof(v.end()), 32);

    /* ra_chunk_like_iterator is one underlying iterator plus three integers, so the deque numbers
     * follow the deque iterator, which is 32 bytes in libstdc++ and 16 in libc++. */
    auto d = std::ref(deq) | radr::chunk(2);
#ifdef _LIBCPP_VERSION
    EXPECT_EQ(sizeof(d), 80);
    EXPECT_EQ(sizeof(d.begin()), 40);
    EXPECT_EQ(sizeof(d.end()), 40);
#else
    EXPECT_EQ(sizeof(d), 112);
    EXPECT_EQ(sizeof(d.begin()), 56);
    EXPECT_EQ(sizeof(d.end()), 56);
#endif
}

TEST(iterator_size, chunk_ra_unsized)
{
    std::vector<int> vec{};
    std::deque<int>  deq{};

    constexpr auto p = [](auto &&)
    {
        return true;
    };

#ifdef __cpp_lib_ranges_chunk
    {
        auto v = vec | std::views::take_while(p) | std::views::chunk(2);
        EXPECT_EQ(sizeof(v), 16);
        EXPECT_EQ(sizeof(v.begin()), 40);
        EXPECT_EQ(sizeof(v.end()), 1);

        auto d = deq | std::views::take_while(p) | std::views::chunk(2);
        EXPECT_EQ(sizeof(d), 16);
        EXPECT_EQ(sizeof(d.begin()), 88);
        EXPECT_EQ(sizeof(d.end()), 1);
    }
#endif

    auto v = std::ref(vec) | radr::take_while(p) | radr::chunk(2);
    EXPECT_EQ(sizeof(v), 32);
    EXPECT_EQ(sizeof(v.begin()), 32);
    EXPECT_EQ(sizeof(v.end()), 1);

    // unidi_chunk_like_iterator stores three deque iterators (underlying end, chunk begin, chunk end)
    auto d = std::ref(deq) | radr::take_while(p) | radr::chunk(2);
#ifdef _LIBCPP_VERSION
    EXPECT_EQ(sizeof(d), 56);
    EXPECT_EQ(sizeof(d.begin()), 56);
#else
    EXPECT_EQ(sizeof(d), 104);
    EXPECT_EQ(sizeof(d.begin()), 104);
#endif
    EXPECT_EQ(sizeof(d.end()), 1);
}

/* lazy_chunk_like_iterator stores the underlying end, the current chunk's begin and n -- it never
 * computes the chunk's end, so compared to unidi_chunk_like_iterator (which also stores that end) it
 * saves one underlying iterator, and compared to bidi_chunk_like_iterator it saves two.
 *
 * Against ra_chunk_like_iterator (one iterator plus three integers) the comparison flips as soon as
 * the underlying iterator gets fat: see the deque numbers below. */

TEST(iterator_size, lazy_chunk_unidi)
{
    std::forward_list<int> vec{};

    // std::views::chunk is the closest equivalent; see chunk_unidi above for its numbers
    auto v = std::ref(vec) | radr::lazy_chunk(2);
    EXPECT_EQ(sizeof(v), 24); // un-sized, so no size stored either
    EXPECT_EQ(sizeof(v.begin()), 24);
    EXPECT_EQ(sizeof(v.end()), 1); // un-common, so we save empty sentinel

    // radr::chunk is 32 here: unidi_chunk_like_iterator stores the chunk's end on top of that
}

TEST(iterator_size, lazy_chunk_bidi_common)
{
    std::list<int> l{};

    auto v = std::ref(l) | radr::lazy_chunk(2);
    EXPECT_EQ(sizeof(v), 32); // + 8 for size
    EXPECT_EQ(sizeof(v.begin()), 24);
    EXPECT_EQ(sizeof(v.end()), 1);

    // radr::chunk is 88/40/40 here, because it uses bidi_chunk_like_iterator and becomes common
}

TEST(iterator_size, lazy_chunk_ra_sized)
{
    std::vector<int> vec{};
    std::deque<int>  deq{};

    auto v = std::ref(vec) | radr::lazy_chunk(2);
    EXPECT_EQ(sizeof(v), 32); // + 8 for size
    EXPECT_EQ(sizeof(v.begin()), 24);
    EXPECT_EQ(sizeof(v.end()), 1);

    // radr::chunk is 64/32/32 here, i.e. its iterator is bigger and it stores two of them

    /* Two deque iterators plus n, so these numbers follow the deque iterator: 32 bytes in libstdc++,
     * 16 in libc++. Note that this iterator is *bigger* than radr::chunk's ra_chunk_like_iterator
     * under libstdc++ (72 vs 56), and the same size under libc++ (40 vs 40). */
    auto d = std::ref(deq) | radr::lazy_chunk(2);
#ifdef _LIBCPP_VERSION
    EXPECT_EQ(sizeof(d), 48);
    EXPECT_EQ(sizeof(d.begin()), 40);
#else
    EXPECT_EQ(sizeof(d), 80);
    EXPECT_EQ(sizeof(d.begin()), 72);
#endif
    EXPECT_EQ(sizeof(d.end()), 1);
}

TEST(iterator_size, lazy_chunk_ra_unsized)
{
    std::vector<int> vec{};
    std::deque<int>  deq{};

    constexpr auto p = [](auto &&)
    {
        return true;
    };

    auto v = std::ref(vec) | radr::take_while(p) | radr::lazy_chunk(2);
    EXPECT_EQ(sizeof(v), 24);
    EXPECT_EQ(sizeof(v.begin()), 24);
    EXPECT_EQ(sizeof(v.end()), 1);

    // radr::chunk is 32/32/1 here; std::views::chunk 16/40/1 (see chunk_ra_unsized above)

    // here radr::chunk is the bigger one on both implementations (104 vs 72, and 56 vs 40)
    auto d = std::ref(deq) | radr::take_while(p) | radr::lazy_chunk(2);
#ifdef _LIBCPP_VERSION
    EXPECT_EQ(sizeof(d), 40);
    EXPECT_EQ(sizeof(d.begin()), 40);
#else
    EXPECT_EQ(sizeof(d), 72);
    EXPECT_EQ(sizeof(d.begin()), 72);
#endif
    EXPECT_EQ(sizeof(d.end()), 1);
}

#if RADR_FEATURE_ZIP

/* radr::adjacent<N> stores N copies of the underlying iterator, so its iterator grows linearly in N.
 * For random-access ranges the copies are redundant (they differ by a compile-time constant offset) and a
 * single-iterator representation would make these numbers constant in N; this test pins down the status quo,
 * so that such a change becomes visible here. */

TEST(iterator_size, adjacent_contig)
{
    std::vector<int> vec{};

#    ifdef __cpp_lib_ranges_zip
    {
        // std stores N iterators, too
        auto v1 = vec | std::views::adjacent<1>;
        auto v2 = vec | std::views::adjacent<2>;
        auto v4 = vec | std::views::adjacent<4>;

        EXPECT_EQ(sizeof(v1), 8);
        EXPECT_EQ(sizeof(v2), 8);
        EXPECT_EQ(sizeof(v4), 8);

        EXPECT_EQ(sizeof(v1.begin()), 8);
        EXPECT_EQ(sizeof(v2.begin()), 16);
        EXPECT_EQ(sizeof(v4.begin()), 32);

        EXPECT_EQ(sizeof(v1.end()), 8);
        EXPECT_EQ(sizeof(v2.end()), 16);
        EXPECT_EQ(sizeof(v4.end()), 32);
    }
#    endif

    {
        auto v1 = std::ref(vec) | radr::adjacent<1>;
        auto v2 = std::ref(vec) | radr::adjacent<2>;
        auto v4 = std::ref(vec) | radr::adjacent<4>;

        // begin and end are both zip_iterators; the size is derived from them, not stored
        EXPECT_EQ(sizeof(v1), 16);
        EXPECT_EQ(sizeof(v2), 32);
        EXPECT_EQ(sizeof(v4), 64);

        EXPECT_EQ(sizeof(v1.begin()), 8);
        EXPECT_EQ(sizeof(v2.begin()), 16);
        EXPECT_EQ(sizeof(v4.begin()), 32);

        EXPECT_EQ(sizeof(v1.end()), 8);
        EXPECT_EQ(sizeof(v2.end()), 16);
        EXPECT_EQ(sizeof(v4.end()), 32);
    }
}

TEST(iterator_size, adjacent_bidi)
{
    std::list<int> l{};

#    ifdef __cpp_lib_ranges_zip
    {
        auto v2 = l | std::views::adjacent<2>;
        auto v4 = l | std::views::adjacent<4>;

        EXPECT_EQ(sizeof(v2), 8);
        EXPECT_EQ(sizeof(v4), 8);

        EXPECT_EQ(sizeof(v2.begin()), 16);
        EXPECT_EQ(sizeof(v4.begin()), 32);

        EXPECT_EQ(sizeof(v2.end()), 16);
        EXPECT_EQ(sizeof(v4.end()), 32);
    }
#    endif

    {
        // bidi + common: end is a zip_iterator, too, but the size has to be stored (+8)
        auto v2 = std::ref(l) | radr::adjacent<2>;
        auto v4 = std::ref(l) | radr::adjacent<4>;

        EXPECT_EQ(sizeof(v2), 40);
        EXPECT_EQ(sizeof(v4), 72);

        EXPECT_EQ(sizeof(v2.begin()), 16);
        EXPECT_EQ(sizeof(v4.begin()), 32);

        EXPECT_EQ(sizeof(v2.end()), 16);
        EXPECT_EQ(sizeof(v4.end()), 32);
    }
}

#endif
