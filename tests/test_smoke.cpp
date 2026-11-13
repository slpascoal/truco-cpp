#include "test_framework.hpp"

TEST(harness_runs) {
    CHECK(1 + 1 == 2);
}

TEST(harness_compares_values) {
    CHECK_EQ(40, 10 * 4);
    CHECK_NE(3, 4);
}
