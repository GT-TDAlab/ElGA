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

#ifndef INTEGER_HASH_HPP
#define INTEGER_HASH_HPP

#include <cstdint>

namespace hashing {
    /** @brief Return a (~uniformly) hashed integer */
    uint64_t hash(uint64_t i);
}

#endif
