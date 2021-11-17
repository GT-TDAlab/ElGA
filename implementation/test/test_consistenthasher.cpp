/**
 * Test files for consistent hasher
 * 
 * Author: Kaan Sancak, Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "tests.hpp"

#include "consistenthasher.hpp"
#include "replicationmap.hpp"


int test_empty(){
    int ret = 0;

    std::vector<uint64_t> agents {};
    NoReplication rm {};

    ConsistentHasher ch(agents, rm);

    auto response = ch.find(1);
    ASSERTEQ(response.size(), 0)

    bool have_ownership = true;
    ASSERTEQ(ch.find_one(10, 3, have_ownership), 0);
    ASSERTEQ(have_ownership, false);
    ASSERTEQ(ch.find_one(10, 0, have_ownership), 0);
    ASSERTEQ(have_ownership, false);

    return ret;
}

int test_find_big() {
    int ret = 0;

    // Create count sketch
    CMSReplicationMap cs {};

    // Make insertions
    // Replication for these values should be 4
    for (size_t i = 0; i < 12000; i++){
        cs.count(2);
        cs.count(4);
        cs.count(6);
        cs.count(8);
        cs.count(9);
    }

    for (size_t i = 0; i < 3000; i++)
        cs.count(3);

    // Replication for this should be 8
    for (size_t i = 0; i < 24000; i++)
        cs.count(10);

    // Create agents
    std::vector<uint64_t> agents = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Create Consistent Hasher
    ConsistentHasher ch(agents, cs);

    // Query a value
    // Get response list & loop through the list check one by one
    auto response = ch.find(1);
    ASSERTEQ(response.size(), 1)

    return ret;
}

int test_find(){
    int ret = 0;

    CMSReplicationMap cs {};


    for (size_t i = 0; i < 6000; i++){
        cs.count(3);
        cs.count(4);
    }

    for (size_t i = 0; i < 3000; i++)
        cs.count(2);

    for (size_t i = 0; i < 12000; i++)
        cs.count(5);

    // Create agents
    std::vector<uint64_t> agents = {1, 2, 3, 4, 5};
    ConsistentHasher ch(agents, cs);

    auto response = ch.find(1);
    ASSERTEQ(response.size(), 1)

    return ret;
}

int test_findone(){
    int ret = 0;
    CMSReplicationMap cs {};


    for (size_t i = 0; i < 6000; i++){
        cs.count(3);
        cs.count(4);
    }

    for (size_t i = 0; i < 3000; i++)
        cs.count(2);

    for (size_t i = 0; i < 12000; i++)
        cs.count(5);

    // Create agents
    std::vector<uint64_t> agents = {1, 2, 3, 4, 5};
    ConsistentHasher ch(agents, cs);

    bool have = true;
    ASSERTEQ(ch.find_one(2, 0, have), 2);
    ASSERTEQ(have, false);
    ASSERTEQ(ch.find_one(2, 2, have), 2);
    ASSERTEQ(have, true);
    have = false;
    ASSERTEQ(ch.find_one(2, 2, have), 2);
    ASSERTEQ(have, true);

    return ret;
}

int test_findone_ignorehigh(){
    int ret = 0;
    CMSReplicationMap cs {};


    for (size_t i = 0; i < 6000; i++){
        cs.count(3);
        cs.count(4);
    }

    for (size_t i = 0; i < 3000; i++)
        cs.count(2);

    for (size_t i = 0; i < 12000; i++)
        cs.count(5);

    // Create agents
    std::vector<uint64_t> agents = {(4llu | (1llu << 49))};
    ConsistentHasher ch(agents, cs);

    bool have = true;
    ASSERTEQ(ch.find_one(2, 0, have), (4 | (1llu << 49)));
    ASSERTEQ(have, false);
    ch.find_one(2, 4, have);
    ASSERTEQ(have, true);

    return ret;
}

int main(int argc, char **argv) {
    int ret = 0;

    RUN_TEST(test_empty)
    RUN_TEST(test_find_big)
    RUN_TEST(test_find)
    RUN_TEST(test_findone)
    RUN_TEST(test_findone_ignorehigh)

    return ret;
}
