/** Test files for count-sketch merge
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

#include <fstream>
#include <unordered_map>
#include <cmath>
#include <iostream>

#include "tests.hpp"


void parse_file(std::string filename, CountSketch &cm){

    std::fstream reader;
    uint64_t cur;
    char buf[4096];
    reader.rdbuf()->pubsetbuf(buf, sizeof(buf));

    reader.open(filename);
    if (!reader.is_open())
        throw std::runtime_error("File not found.");

    while (!reader.eof() && reader.peek() != -1) {
        reader >> cur;
        if (reader.fail())
                throw std::runtime_error("Parsing error.");

        if (reader.peek() == '\n')
            reader.get();
        else
            throw std::runtime_error("Bad file format.");

        cm.count(cur);

    }

    reader.close();
}

int test_countsketch_merge() {
    CountSketch cm_A;
    CountSketch cm_B;
    CountSketch cm_M;

    // Read the files
    parse_file("zipf/data_1k_1", cm_A);
    parse_file("zipf/data_1k_1", cm_M);
    parse_file("zipf/data_1k_2", cm_B);
    parse_file("zipf/data_1k_2", cm_M);

    // Merge sketches and maps
    cm_A.merge(cm_B);

    int is_same = cm_A == cm_M;
    ASSERTEQ(is_same, true)

    return 0;
}


int main(int argc, char **argv) {

    int ret = 0;
    RUN_TEST(test_countsketch_merge)

    return ret;
}
