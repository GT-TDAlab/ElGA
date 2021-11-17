/**
 * ElGA Count Sketch Class
 * This is an implementation of Count Sketch
 * Reference: https://dl.acm.org/citation.cfm?id=684566
 *
 * Author: Kaan Sancak
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "countsketchbase.hpp"
#include "integer_hash.hpp"

#include <cmath>
#include <ctime>

class CountSketch : public CountSketchBase {

    private:
        void init_table();

    protected:
        const static uint64_t TABLE_SIZE = TABLE_WIDTH*TABLE_DEPTH;

        struct SharedTable{
            int32_t data[TABLE_SIZE];
        };

        std::shared_ptr<SharedTable> table;

    public:
        CountSketch();
        CountSketch(const char * in);
        void clear();
        void count(uint64_t key);
        void merge(CountSketchBase &cs);
        char* serialize();
        int32_t query(uint64_t key) const;
        void update(const char* data);
        void test_count(uint64_t key, int64_t &max, int64_t &min);
        int64_t hash(uint64_t key, int64_t index) const;
        int16_t hash_sign(uint64_t key, int64_t index) const;
        bool operator==(const CountSketch& rhs) const;
        uint32_t median(int32_t res[]) const;

        static const size_t size() { return sizeof(SharedTable); }
};
