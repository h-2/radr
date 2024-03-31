#pragma once

#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <typeinfo>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#    include <cxxabi.h>
#endif // defined(__GNUC__) || defined(__clang__)

#include <gtest/gtest.h>

namespace radr::test
{

// --------------------------------------------------------------------------
// EXPECT_RANGE_EQ
// --------------------------------------------------------------------------

#define EXPECT_RANGE_EQ(val1, val2) EXPECT_PRED_FORMAT2(::radr::test::expect_range_eq{}, val1, val2);

struct expect_range_eq
{
    template <typename rng_t>
    auto copy_range(rng_t && rng)
    {
        using value_t = std::ranges::range_value_t<rng_t>;
        std::vector<value_t> rng_copy{};
        std::ranges::copy(rng, std::back_inserter(rng_copy));
        return rng_copy;
    }

    template <std::ranges::range lhs_t, std::ranges::range rhs_t>
    ::testing::AssertionResult operator()(char const * lhs_expression,
                                          char const * rhs_expression,
                                          lhs_t &&     lhs,
                                          rhs_t &&     rhs)
    {
        std::vector lhs_copy = copy_range(lhs);
        std::vector rhs_copy = copy_range(rhs);

        if (std::ranges::equal(lhs_copy, rhs_copy))
            return ::testing::AssertionSuccess();

        return ::testing::internal::CmpHelperEQFailure(lhs_expression, rhs_expression, lhs_copy, rhs_copy);
    }
};

// --------------------------------------------------------------------------
// EXPECT_SAME_TYPE
// --------------------------------------------------------------------------

// https://stackoverflow.com/a/62984543
#define EXPECT_SAME_TYPE_DEPAREN(X) EXPECT_SAME_TYPE_ESC(EXPECT_SAME_TYPE_ISH X)
#define EXPECT_SAME_TYPE_ISH(...)   EXPECT_SAME_TYPE_ISH __VA_ARGS__
#define EXPECT_SAME_TYPE_ESC(...)   EXPECT_SAME_TYPE_ESC_(__VA_ARGS__)
#define EXPECT_SAME_TYPE_ESC_(...)  EXPECT_SAME_TYPE_VAN##__VA_ARGS__
#define EXPECT_SAME_TYPE_VANEXPECT_SAME_TYPE_ISH

#define EXPECT_SAME_TYPE(val1, val2)                                                                                   \
    EXPECT_PRED_FORMAT2(::radr::test::expect_same_type<true>{},                                                        \
                        (std::type_identity<EXPECT_SAME_TYPE_DEPAREN(val1)>{}),                                        \
                        (std::type_identity<EXPECT_SAME_TYPE_DEPAREN(val2)>{}));

#define EXPECT_DIFFERENT_TYPE(val1, val2)                                                                              \
    EXPECT_PRED_FORMAT2(::radr::test::expect_same_type<false>{},                                                       \
                        (std::type_identity<EXPECT_SAME_TYPE_DEPAREN(val1)>{}),                                        \
                        (std::type_identity<EXPECT_SAME_TYPE_DEPAREN(val2)>{}));

template <bool outcome = true>
struct expect_same_type
{
    template <typename lhs_t, typename rhs_t>
    ::testing::AssertionResult operator()(std::string_view          lhs_expression,
                                          std::string_view          rhs_expression,
                                          std::type_identity<lhs_t> lhs,
                                          std::type_identity<rhs_t> rhs)
    {
        auto remove_wrap_type_identity = [](std::string_view str)
        {
            // EXPECT_SAME_TYPE_DEPAREN adds a space after the prefix
            static constexpr std::string_view prefix = "(std::type_identity< ";
            static constexpr std::string_view suffix = ">{})";

            if (str.starts_with(prefix))
                str.remove_prefix(prefix.size());
            if (str.ends_with(suffix))
                str.remove_suffix(suffix.size());
            return static_cast<std::string>(str);
        };

        auto type_name_as_string = []<typename type>(std::type_identity<type>)
        {
            std::string demangled_name{};
#if defined(__GNUC__) || defined(__clang__) // clang and gcc only return a mangled name.
            using safe_ptr_t = std::unique_ptr<char, std::function<void(char *)>>;

            int        status{};
            safe_ptr_t demangled_name_ptr{abi::__cxa_demangle(typeid(type).name(), 0, 0, &status),
                                          [](char * name_ptr)
            {
                free(name_ptr);
            }};
            demangled_name = std::string{std::addressof(*demangled_name_ptr)};
#else  // e.g. MSVC
            demangled_name = typeid(type).name();
#endif // defined(__GNUC__) || defined(__clang__)

            if constexpr (std::is_const_v<std::remove_reference_t<type>>)
                demangled_name += " const";
            if constexpr (std::is_lvalue_reference_v<type>)
                demangled_name += " &";
            if constexpr (std::is_rvalue_reference_v<type>)
                demangled_name += " &&";

            return demangled_name;
        };

        if (std::is_same_v<lhs_t, rhs_t> == outcome)
            return ::testing::AssertionSuccess();

        if (outcome)
        {
            return ::testing::internal::CmpHelperEQFailure(remove_wrap_type_identity(lhs_expression).c_str(),
                                                           remove_wrap_type_identity(rhs_expression).c_str(),
                                                           type_name_as_string(lhs),
                                                           type_name_as_string(rhs));
        }
        else
        {
            return ::testing::internal::CmpHelperNE(remove_wrap_type_identity(lhs_expression).c_str(),
                                                    remove_wrap_type_identity(rhs_expression).c_str(),
                                                    type_name_as_string(lhs),
                                                    type_name_as_string(rhs));
        }
    }
};

} // namespace radr::test
