/**
 * ElGA simple test assertions
 * Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef ELGA_TESTS_
#define ELGA_TESTS_

#include <cmath>
#include <iostream>

#define ASSERTEQ(x,y)                                                                   \
    do {                                                                                \
        if ((x) != (y)) {                                                               \
            std::cerr << "TEST FAILURE: Expected: " << y << " got: " << x               \
                << " for: " << #x << " " << __FILE__ << ":" << __LINE__ << std::endl;   \
            return 1;                                                                   \
            }                                                                           \
    } while(0);

// A better implementation could use https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
// or https://github.com/google/googletest/blob/master/googletest/include/gtest/internal/gtest-internal.h
#define ASSERTCLOSE(x,y,eps)                                                            \
    do {                                                                                \
        if (std::fabs(x-y) > eps) {                                                     \
            std::cerr << "TEST FAILURE: Expected: " << y << " got: " << x               \
                << " for: " << #x << " " << __FILE__ << ":" << __LINE__ << std::endl;   \
            return 1;                                                                   \
            }                                                                           \
    } while(0);

#define ASSERTEQNP(x,y)                                                 \
    do {                                                                \
        if (!((x) == (y))) {                                            \
        std::cerr << "TEST FAILURE: For: " << #x << " " << __FILE__     \
            << ":" << __LINE__ << std::endl;                            \
        return 1;                                                       \
        }                                                               \
    } while(0);

#define ASSERTNEQNP(x,y)                                    \
    do {                                                    \
        if ((x) == (y)) {                                   \
            std::cerr << "TEST FAILURE: For: " << #x << " " \
            << __FILE__ << ":" << __LINE__ << std::endl;    \
            return 1;                                       \
        }                                                   \
    } while(0);

#define RUN_TEST(fn)                                                \
    do {                                                            \
        int ret_ = fn();                                            \
        if (ret_ != 0) {                                            \
            std::cerr << "TEST FAILED: " << #fn << std::endl;       \
            } else {                                                \
                std::cout << "TEST PASSED: " << #fn << std::endl;   \
            }                                                       \
            ret |= ret_;                                            \
    } while (0);

#define RUN_TEST_ARGS(fn, ...)                                  \
    do {                                                        \
        int ret_ = fn(__VA_ARGS__);                             \
        if (ret_) {                                             \
            std::cerr << "TEST FAILED: " << #fn << std::endl;   \
        } else {                                                \
            std::cout << "TEST PASSED: " << #fn << std::endl;   \
        }                                                       \
        ret |= ret_;                                            \
    } while(0);

#endif
