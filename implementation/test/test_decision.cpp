/**
 * Test files for decision test
 * between Count Sketch and Count-Min Sketch
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

#include <fstream>
#include <unordered_map>
#include <cmath>
#include <iostream>



int test_from_file(std::string filename, long total_count, int isMin) {
    float epsilon = 0.000075;
    float delta = 0.001;
    uint64_t cur;
    int64_t min = 0;
    int64_t max = 0;

    int64_t err_below = 0;
    int64_t err_above = 0;

    std::unordered_map<uint64_t, int64_t> map;
    std::fstream reader;
    char buf[4096];

    CountMinSketch cm = CountMinSketch();

    reader.rdbuf()->pubsetbuf(buf, sizeof(buf));
    reader.open(filename);

    if (!reader.is_open())
        throw std::runtime_error("Parsing error.");

    while (!reader.eof() && reader.peek() != -1) {
        reader >> cur;
        if (reader.fail())
                throw std::runtime_error("Parsing error.");

        if (reader.peek() == '\n')
            reader.get();
        else
            throw std::runtime_error("Bad file format.");

        cm.test_count(cur, max, min);

        if (map.find(cur) != map.end())
            map[cur]++;
        else
            map[cur] = 1;
    }

    if (isMin)
        std::cerr << "For count min sketch with " << total_count << " insertions:" << std::endl;
    else
        std::cerr << "For count sketch with " << total_count << " insertions:" << std::endl;

    std::cerr << "Watermark Max reached: " << max << std::endl;
    std::cerr << "Watermark Min reached: " << min << std::endl;

    int64_t cum_err = 0;
    int64_t total_err = 0;
    for (const auto& item: map) {
        int64_t estimate = cm.query(item.first);
        int64_t err = estimate - item.second;

        if (err > err_above)
            err_above = err;
        if (err < err_below)
            err_below = err;

        err = std::abs(err);

        if (err != 0)
            total_err++;

        if(err >= total_count * epsilon)
            cum_err += err;
    }

    std::cerr << "Total error: " <<  total_err << std::endl;
    std::cerr << "Maximum reached error: " << err_above << std::endl;
    std::cerr << "Minimum reached error: " << err_below << std::endl;
    std::cerr << "Error is greater than epsilon for:  " <<   cum_err << " queries" << std::endl;
    std::cerr << std::endl;
    return 0;
}


int main(int argc, char **argv) {
    long tests[5] = {100000, 1000000, 10000000, 100000000, 1000000000};
    std::string fnames[5] = {"data_100k", "data_1m", "data_10m", "data_100m", "data_1b"};
    std::string dir_zipf = "./zipf/";

    if (argc == 2)
        dir_zipf = argv[1];
    else
        return 0;

    std::cerr << dir_zipf << std::endl;

    std::cerr << "Running experiments for random distribution: " << std::endl;
    for (int i = 0; i < 4; i++)
            test_from_file( dir_zipf + fnames[i] + "_1", tests[i], 0);

    return 0;
}
