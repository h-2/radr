/* GENERAL INSTRUCTIONS:
 *
 * - use GoogleTest
 * - every test should be short (fit on one page)
 * - but there should be many covering also edge cases
 * - no fixtures or other complicated constructs
 * - checks that can be performed at compile-time, should still be performed at run-time (e.g. concept checks or type-equality checks)
 * - In addition to GoogleTest macros there is EXPECT_RANGE_EQ and EXPECT_SAME_TYPE from gtest_helpers.hpp
 *
 * This file illustrates the general structure of range adaptor unit tests.
 * We are using "foobar" is as a placeholder for the actual adaptor name
 * See the inline `INSTRUCTION` blocks for what to add; you can do this step-by-step.
 *
 * When asked to check "all range concepts", call `check_adaptor_concepts` from
 * `adaptor_template.hpp`.  Only specify concepts that are expected as true
 * (implicit is always false). If the point of a test is proving that a concept check
 * would fail, instead add a comment for this.
 * The documentation at the bottom of the adaptor's header file should explain the expected
 * values for these. If for any reason, the behaviour you observe is different from the
 * documentation, IMMEDIATELY ALERT THE PROGRAMMER. If expected values are unclear, also ask.
 *
 * NEVER CHANGE LIBRARY CODE WHILE WRITING TESTS, UNLESS EXPLICITLY TOLD SO.
 *
 * If an adaptor is documented as being "transparent", also check that
 * the radr::iterator_t and radr::const_iterator_t of the adaptor are the same as for the
 * underlying range.
 */

#include <gtest/gtest.h>
#include <radr/test/gtest_helpers.hpp>
#include <radr/test/adaptor_template.hpp>

// avoid visual clutter
using radr::test::range_cat;

/* INSTRUCTION: add include for the respective adaptor.
 * The header contains valuable documentation of the adaptor towards the end of the file.
 */

// --------------------------------------------------------------------------
// test data
// --------------------------------------------------------------------------

/* INSTRUCTION: add inline data variables that can be re-used.
 * Typically this includes one or more input ranges and an output range with "expected results".
 * Only move data here that is actually used by more than two unit tests.
 */

// --------------------------------------------------------------------------
// single-pass tests
// --------------------------------------------------------------------------

/* INSTRUCTION: if the adaptor does not work on single-pass inputs (see documentation)
 * only add a note that this is the case, and skip this section.
 */


TEST(foobar_sp, simple)
{
    // typical use of the single pass adaptor
    // use radr::iota as input
    // compare output to known correct values

    // check that the returned type is a specialisation of radr::generator
}

TEST(foobar_sp, other)
{
    // add 1 or more extra tests if documentation states any special cases or requirements
}

// --------------------------------------------------------------------------
// multi-pass tests I – canonical cases
// --------------------------------------------------------------------------

/* INSTRUCTION:
 * Cover the most common cases! If an adaptor can't be created on one of the
 * specified inputs, add a note the source-code stating this and skip the
 * respective test.
 *
 * for each of the canonical cases, add a second test that uses an rvalue/copy
 * of the input as underlying range (use `auto(var)` to make copies).
 */

TEST(foobar_mp, forward)
{
    // test with std::ref of std::forward_list as input
    // EXPECT_RANGE_EQ with known good results (→ data section)
    // check all range concepts on the result
}

TEST(foobar_mp, bidi_common)
{
    // test with std::ref of std::list as input
    // EXPECT_RANGE_EQ with known good results (→ data section)
    // check all range concepts on the result
}

TEST(foobar_mp, ra_sized)
{
    // test with std::ref of std::deque as input
    // EXPECT_RANGE_EQ with known good results (→ data section)
    // check all range concepts on the result
}

TEST(foobar_mp, contig_sized)
{
    // test with std::ref of std::vector as input
    // EXPECT_RANGE_EQ with known good results (→ data section)
    // check all range concepts on the result
}

// --------------------------------------------------------------------------
// multi-pass tests II – common edge cases
// --------------------------------------------------------------------------

TEST(foobar_mp, mutate)
{
    // skip if range is never mutable
    // test with std::ref of std::vector as input
    // assign through the adaptor and validate results with EXPECT_RANGE_EQ
    // NO concept tests required here because identical to `contig_sized`
}

TEST(foobar_mp, empty)
{
    // test with std::ref of an empty std::vector as imput
    // EXPECT_RANGE_EQ with expected results (typically also empty)
    // NO concept tests required here because identical to `contig_sized`
}

TEST(foobar_mp, constant)
{
    // test with std::cref of std::vector as input
    // EXPECT_RANGE_EQ with known good results (→ data section)
    // check all range concepts on the result
}

TEST(foobar_mp, infinite)
{
    // test with unbounded radr::iota or radr::repeat as input
    // check first two elements with known good results
    // check all range concepts on the result
}

TEST(foobar_mp, fwd_uncommon)
{
    // test with std::ref of std::forward_list | radr::filter as input
    // EXPECT_RANGE_EQ with known good results (→ data section)
    // check all range concepts on the result
}

TEST(foobar_mp, bidi_uncommon)
{
    // test with std::ref of std::vector | radr::filter as input
    // EXPECT_RANGE_EQ with known good results (→ data section)
    // check all range concepts on the result
}

TEST(foobar_mp, ra_nonsized)
{
    // test with std::ref of std::vector | radr::take_while
    // EXPECT_RANGE_EQ with known good results (→ data section)
    // check all range concepts on the result
}

TEST(foobar_mp, contig_nonsized)
{
    // test with std::ref of std::deque | radr::take_while
    // EXPECT_RANGE_EQ with known good results (→ data section)
    /// check all range concepts on the result
}

// --------------------------------------------------------------------------
// multi-pass tests III – adaptor-specific tests
// --------------------------------------------------------------------------

/* INSTRUCTION:
 * Anything that is adaptor-specific goes here. Examples are:
 * - additional arguments to adaptor
 * - special return types under certain circumstances
 * - special concept interactions not covered previously
 * - an adaptor that has size-dependent behaviour should be tested with size too large/small
 */

// --------------------------------------------------------------------------
// multi-pass tests IV – special owning tests
// --------------------------------------------------------------------------

TEST(foobar_mp, deep_copy)
{
    /* INSTRUCTION:
     * Add a test that checks that owning adaptors are copied properly.
     * This is what it looks like for radr::enumerate:

    using T = decltype(std::vector<std::string>{"foo", "bar", "baz"} | radr::enumerate);
    T cpy;
    {
        T own = std::vector<std::string>{"foo", "bar", "baz"} | radr::enumerate;
        EXPECT_RANGE_EQ(own, comp);
        cpy = own;
    }
    EXPECT_RANGE_EQ(cpy, comp);

    */
}
