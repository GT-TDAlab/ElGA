/**
 * ElGA Count Min Sketch Class
 *
 * Author: Kaan Sancak
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef COUNTMINSKETCH_HPP
#define COUNTMINSKETCH_HPP

#include "countsketch.hpp"

class CountMinSketch : public CountSketch {
    public:
        CountMinSketch();
        CountMinSketch(const char * in);
        void count(uint64_t key);
        void test_count(uint64_t key, int64_t &max, int64_t &min);
        void merge(CountSketchBase &cs);
        void disjoint_merge(CountSketchBase &cs);
        int32_t query(uint64_t key) const;
        int32_t query_count(uint64_t key);
};

#endif
