/**
 * Test files for streamer
 *
 * Author: Kaan Sancak,  Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "tests.hpp"

#include "streamer.hpp"

#include <cstdio>

int test_parse() {
    std::string filename = "test.bin";
    std::fstream s(filename, s.binary | s.trunc | s.in | s.out);
    if (!s.is_open()) {
        std::cout << "failed to open " << filename << '\n';
    } else {
        s << "+1 1 5 5.1 1048757088\n"<< "-1 71 77 -25.3 1167247890";
        s.seekp(0);

        std::tuple<edge_t, bool> test_tuple = elga::streamer::parse_edge(s);
        // Remove the newline character
        s.get();
        std::tuple<edge_t, bool> test_tuple2 = elga::streamer::parse_edge(s);

        ASSERTEQ(std::get<0>(test_tuple).src, 1)
        ASSERTEQ(std::get<0>(test_tuple2).src, 71)
        ASSERTEQ(std::get<0>(test_tuple).dst, 5)
        ASSERTEQ(std::get<0>(test_tuple2).dst, 77)
        /*
        ASSERTCLOSE(std::get<0>(test_tuple).weight, 5.1, 1e-10)
        ASSERTCLOSE(std::get<0>(test_tuple2).weight, -25.3, 1e-10)
        ASSERTEQ(std::get<0>(test_tuple).timestamp, 1048757088)
        ASSERTEQ(std::get<0>(test_tuple2).timestamp, 1167247890)
        */
        ASSERTEQ(std::get<1>(test_tuple), true)
        ASSERTEQ(std::get<1>(test_tuple2), false)
    }

    return 0;
}

int main(int argc, char **argv) {
    int ret = 0;

    try {

        RUN_TEST(test_parse)

    } catch (...) {
        // Cleanup the file
        remove("test.bin");
        throw;
    }

    remove("test.bin");

    return ret;
}
