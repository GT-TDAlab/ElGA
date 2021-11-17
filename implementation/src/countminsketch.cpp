/**
 * ElGA Count-Min Sketch
 *
 * Author: Kaan Sancak
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "countminsketch.hpp"
#include <iostream>

CountMinSketch::CountMinSketch() : CountSketch() {
}

CountMinSketch::CountMinSketch(const char * in) : CountSketch(in) {}

void CountMinSketch::count(uint64_t key) {
    for (int64_t i = 0; i < TABLE_DEPTH; i++)
        table->data[i * TABLE_WIDTH + hash(key, i)]++;
}


int32_t CountMinSketch::query(uint64_t key) const {
    static_assert(TABLE_DEPTH > 0);
    int32_t min = table->data[hash(key, 0)];
    for (uint32_t i = 1; i < TABLE_DEPTH; i++) {
        int32_t this_val = table->data[i * TABLE_WIDTH + hash(key, i)];
        if (this_val < min) min = this_val;
    }
    return min;
}

int32_t CountMinSketch::query_count(uint64_t key) {
    static_assert(TABLE_DEPTH > 0);
    int32_t min = ++table->data[hash(key, 0)];
    for (uint32_t i = 1; i < TABLE_DEPTH; i++) {
        int32_t this_val = ++table->data[i * TABLE_WIDTH + hash(key, i)];
        if (this_val < min) min = this_val;
    }
    return min;
}

void CountMinSketch::merge(CountSketchBase &cs) {
    CountMinSketch *other = static_cast<CountMinSketch *>(&cs);
    for (uint32_t i = 0; i < TABLE_DEPTH * TABLE_WIDTH; i++)
            table->data[i] += other->table->data[i];
}

void CountMinSketch::disjoint_merge(CountSketchBase &cs) {
    CountMinSketch *other = static_cast<CountMinSketch *>(&cs);
    for (uint32_t i = 0; i < TABLE_DEPTH * TABLE_WIDTH; i++)
            table->data[i] = std::max(table->data[i], other->table->data[i]);
}

void CountMinSketch::test_count(uint64_t key, int64_t &max, int64_t &min){
    for (int64_t i = 0; i < TABLE_DEPTH; i++) {
        table->data[i * TABLE_WIDTH + hash(key, i)]++;
        int32_t value = table->data[i * TABLE_WIDTH + hash(key, i)];
        if(value > max)
            max = value;
        else if (value < min)
            min = value;
    }
}
