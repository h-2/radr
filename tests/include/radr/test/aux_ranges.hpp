#pragma once

#include <radr/generator.hpp>

namespace radr::test
{

inline radr::generator<size_t> iota_input_range(size_t const b, size_t const e)
{
    for (size_t i = b; i < e; ++i)
        co_yield i;
}

} // namespace radr::test
