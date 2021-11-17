/**
 * ElGA Count Sketch Base Class
 *
 * This is an abstract class for count sketches.
 * Count sketches will be used as frequency tables
 * for streaming data, using sublinear makpace.
 *
 * Author: Kaan Sancak
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include <memory>
#include <algorithm>
#include <cstring>

class CountSketchBase {
    public:
        virtual void count(uint64_t key) = 0;
        virtual int32_t query(uint64_t key) const = 0;
        virtual void merge(CountSketchBase &cs) = 0;
        virtual char* serialize() = 0;
};
