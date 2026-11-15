#include "../../src/common/fixed_string.hpp"
#include "../release_assert.hpp"
#include <cassert>
#include <string_view>

using namespace common;

namespace {
    void test_default_constructor() {
        FixedString<16> fs;

        TEST_ASSERT(fs.empty());
        TEST_ASSERT(fs.size() == 0);
        TEST_ASSERT(fs.capacity() == 16);
        TEST_ASSERT(fs.data()[0] == '\0');
    }

    void test_string_view_constructor() {
        std::string_view source{"APPL"};
        FixedString<8> fs{source};

        TEST_ASSERT(!fs.empty());
        TEST_ASSERT(fs.size() == 4);
        TEST_ASSERT(fs.to_string_view() == "APPL");
        TEST_ASSERT(fs.data()[4] == '\0');
    }

    void test_trunction() {
        std::string_view test_id{"ORD123456789"};
        FixedString<8> fs{test_id};

        TEST_ASSERT(fs.size() == 8);
        TEST_ASSERT(fs.to_string_view() == "ORD12345");
        TEST_ASSERT(fs.capacity() == 8);
    }

    void test_equality_operators() {
        FixedString<16> fs1{"BTC_USD"};
        FixedString<16> fs2{"BTC_USD"};
        FixedString<16> fs3{"ETH_USD"};

        TEST_ASSERT(fs1 == fs2);
        TEST_ASSERT(!(fs1 == fs3));
        TEST_ASSERT(fs1 == "BTC_USD");
        TEST_ASSERT(fs3 == "ETH_USD");
    }

    void test_clear() {
        FixedString<16> fs1{"BTC_USD"};

        TEST_ASSERT(fs1 == "BTC_USD");
        fs1.clear();
        TEST_ASSERT(fs1 == "");
    }

    void test_assignment_operator() {
        FixedString<16> fs;
        fs = "string1";
        TEST_ASSERT(fs.size() == 7);
        TEST_ASSERT(fs.to_string_view() == "string1");
        TEST_ASSERT(fs.data()[7] == '\0');

        std::string_view sv{"string2"};
        fs = sv;
        TEST_ASSERT(fs.size() == 7);
        TEST_ASSERT(fs.to_string_view() == "string2");
        TEST_ASSERT(fs.data()[7] == '\0');

        FixedString<8> fs_small;
        fs_small = "very_long_string_id";
        TEST_ASSERT(fs_small.size() == 8);
        TEST_ASSERT(fs_small.to_string_view() == "very_lon");

        fs = "str";
        TEST_ASSERT(fs.size() == 3);
        TEST_ASSERT(fs.to_string_view() == "str");
        TEST_ASSERT(fs.data()[3] == '\0');

        fs = "";
        TEST_ASSERT(fs.empty());
        TEST_ASSERT(fs.size() == 0);
        TEST_ASSERT(fs.data()[0] == '\0');
    }
} // namespace

void run_fixed_string_unit_tests() {
    test_default_constructor();
    test_string_view_constructor();
    test_trunction();
    test_equality_operators();
    test_clear();
    test_assignment_operator();
}