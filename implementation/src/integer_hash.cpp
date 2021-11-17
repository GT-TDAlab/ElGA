/**
 * ElGA integer hashing
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "integer_hash.hpp"

#ifdef CONFIG_USE_ABSEIL
#include "absl/hash/hash.h"
#endif
#ifdef CONFIG_USE_CRC
// From e.g., https://github.com/srned/baselib
#include "crc64.hpp"
#endif

namespace hashing {
    uint64_t hash(uint64_t i) {
        #ifdef CONFIG_USE_CRC
        return crc64((char*)&i, sizeof(i));
        #else
        #ifdef CONFIG_USE_ABSEIL
        auto hasher = absl::Hash<uint64_t>();
        return hasher(i);
        #else
        uint64_t x = i;
        #ifdef CONFIG_USE_WANG
        x = (~x) + (x << 21); // x = (x << 21) - x - 1;
        x = x ^ (x >> 24);
        x = (x + (x << 3)) + (x << 8); // x * 265
        x = x ^ (x >> 14);
        x = (x + (x << 2)) + (x << 4); // x * 21
        x = x ^ (x >> 28);
        x = x + (x << 31);
        #else
        // See:
        // https://www.reddit.com/r/RNG/comments/jqnq20/the_wang_and_jenkins_integer_hash_functions_just/
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9;
        x ^= x >> 27;
        x *= 0x94d049bb133111eb;
        x ^= x >> 31;
        #endif
        return x;
        #endif
        #endif
    }
}
