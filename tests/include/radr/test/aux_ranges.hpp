#pragma once

#include <algorithm>
#include <random>

#include <radr/generator.hpp>

namespace radr::test
{

inline radr::generator<size_t> iota_input_range(size_t const b, size_t const e)
{
    for (size_t i = b; i < e; ++i)
        co_yield i;
}

template <typename number_type>
auto generate_numeric_sequence(size_t const      len  = 500,
                               number_type const min  = std::numeric_limits<number_type>::lowest(),
                               number_type const max  = std::numeric_limits<number_type>::max(),
                               size_t const      seed = 0)
{
    std::mt19937_64                       engine(seed);
    std::uniform_int_distribution<size_t> dist{min, max};

    auto gen = [&dist, &engine]()
    {
        return dist(engine);
    };
    std::vector<number_type> sequence(len);
    std::ranges::generate(sequence, gen);

    return sequence;
}

} // namespace radr::test
