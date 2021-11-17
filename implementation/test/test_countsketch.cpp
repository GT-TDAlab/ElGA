/** Test files for count-sketch
 *
 * Author: Kaan Sancak
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "tests.hpp"

#include "countsketch.hpp"

#include <fstream>
#include <unordered_map>
#include <cmath>

int test_insert_same_and_check(){
    CountSketch cm;

    for(uint64_t i = 0; i < 20; i++) {
        cm.count(20);
        ASSERTEQ(cm.query(20), i+1)
    }
    return 0;
}

int test_seriliaze_deserialize(){
    CountSketch cm;

    for(uint64_t i = 0; i < 500; i++){
        cm.count(20);
        ASSERTEQ(cm.query(20), i+1)
    }

    char* out = cm.serialize();

    CountSketch other(out);

    bool is_same = cm == other;
    ASSERTEQ(is_same, true)
    ASSERTEQ(other.query(20), 500)

    return 0;
}

int test_update(){
    CountSketch cm {};
    CountSketch other {};


    for(uint64_t i = 0; i < 500; i++){
        cm.count(20);
        ASSERTEQ(cm.query(20), i+1)
    }

    for(uint64_t i = 0; i < 500; i++){
        other.count(30);
        ASSERTEQ(other.query(30), i+1)
    }

    bool is_same = cm == other;
    ASSERTEQ(is_same, false)

    char *out = cm.serialize();

    other.update(out);

    is_same = cm == other;
    ASSERTEQ(is_same, true)

    return 0;
}


int main(int argc, char **argv) {
    int ret = 0;

    RUN_TEST(test_insert_same_and_check)
    RUN_TEST(test_seriliaze_deserialize)
    RUN_TEST(test_update)

    return ret;
}
