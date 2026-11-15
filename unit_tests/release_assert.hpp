#pragma once
#include <cstdlib>
#include <iostream>

// clang-format off
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "Test failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << '\n'; \
            std::abort(); \
        } \
    } while (false)
// clang-format on