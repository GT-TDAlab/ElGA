/**
 * Test files for the integer hash
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "tests.hpp"
#include "integer_hash.hpp"

int test_hashing() {
    uint64_t val = 0llu;
    uint64_t a = hashing::hash(val);

    if (a != 8633297058295171728llu) {
        std::cerr << a << std::endl;
        ASSERTEQ(1,0);
    }

    return 0;
}

int test_diff() {
    uint64_t a = hashing::hash(0llu);
    uint64_t b = hashing::hash(1llu);
    if (a < b) std::swap(a,b);
    if (a - b < 100000000) ASSERTEQ(1,0);
    return 0;
}

int main(int, char**) {
    int ret = 0;

    RUN_TEST(test_hashing);
    RUN_TEST(test_diff);

    return ret;
}
