/**
 * ElGA Count Sketch Class
 *
 * Author: Kaan Sancak
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "countsketch.hpp"
#include <iostream>

CountSketch::CountSketch(const char * in) {
    init_table();
    memcpy(table.get(), (SharedTable *)in, sizeof(SharedTable));
}

CountSketch::CountSketch() {
    init_table();
    clear();
}

void CountSketch::clear() {
    memset(table.get(), 0, sizeof(SharedTable));
}

void CountSketch::init_table() {
    table = std::make_shared<SharedTable>();
}

int64_t CountSketch::hash(uint64_t key, int64_t index) const {
    return hashing::hash(key ^ index) & (TABLE_WIDTH - 1);
}

int16_t CountSketch::hash_sign(uint64_t key, int64_t index) const {
   return (hashing::hash(key ^ (TABLE_DEPTH + index) ) & 1) == 0 ? 1 : - 1;
}

// Finds the median of a given array
uint32_t CountSketch::median(int32_t res[]) const {
    std::sort(res, res + TABLE_DEPTH);
    int32_t mid = TABLE_DEPTH >> 1;

    if((TABLE_DEPTH & 1) == 1)
        return res[mid];

    return (res[mid-1] + res[mid]) >> 1;
}

// Insert a given value using hash functionss
void CountSketch::count(uint64_t key) {
    for (int64_t i = 0; i < TABLE_DEPTH; i++)
        table->data[i * TABLE_WIDTH + hash(key, i)] += hash_sign(key, i);
}


// Query a given value and return median
int32_t CountSketch::query(uint64_t key) const {
    int32_t res[TABLE_DEPTH];
    for (int32_t i = 0; i < TABLE_DEPTH; i++)
        res[i] = table->data[i * TABLE_WIDTH + hash(key, i)] * hash_sign(key, i);

    return median(res);
}

void CountSketch::update(const char* data){
    std::memcpy(table.get(), (const SharedTable *)data, sizeof(SharedTable));
}

void CountSketch::test_count(uint64_t key, int64_t &max, int64_t &min){
    for (int64_t i = 0; i < TABLE_DEPTH; i++) {
        table->data[i * TABLE_WIDTH + hash(key, i)] += hash_sign(key, i);
        int32_t value = table->data[i * TABLE_WIDTH + hash(key, i)];
        if(value > max)
            max = value;
        else if (value < min)
            min = value;
    }
}

void CountSketch::merge(CountSketchBase &cs) {
    CountSketch *other = static_cast<CountSketch *>(&cs);
    for (uint64_t i = 0; i < TABLE_DEPTH * TABLE_WIDTH; i++)
            table->data[i] += other->table->data[i];
}

bool CountSketch::operator==(const CountSketch& rhs) const{
    return (std::memcmp(table.get(), rhs.table.get(), sizeof(SharedTable)) == 0);
};

// Return row pointer for unique_ptr
char* CountSketch::serialize() {
    return (char*)table.get();
}
