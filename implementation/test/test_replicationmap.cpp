/**
 * Test files for replication map
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
#include "replicationmap.hpp"

int test_cs_onerep() {

    CSReplicationMap cs_rm;

    ASSERTEQ(cs_rm.query(20), 1)

    return 0;
}

int test_cs_replication() {

    CSReplicationMap cs_rm;

    for(uint64_t i = 0; i < 2; i++) {
        for (size_t ctr = 0; ctr < 1000; ++ctr)
            cs_rm.count(20);
        ASSERTEQ(cs_rm.sk_query(20), 1000*(i+1))
    }

    return 0;
}

int test_cms_replication() {

    CMSReplicationMap cms_rm;

    for(uint64_t i = 0; i < 2; i++) {
        for (size_t ctr = 0; ctr < 1000; ++ctr)
            cms_rm.count(20);
        cms_rm.count(20);
        ASSERTEQ(cms_rm.sk_query(20), 1000*(i+1)+i+1)
    }

    return 0;
}

int test_no_replication() {

    NoReplication nr;

    ASSERTEQ(nr.query(20), 1)

    return 0;
}


int main(int argc, char **argv) {
    int ret = 0;

    RUN_TEST(test_cs_onerep)
    RUN_TEST(test_cs_replication)
    RUN_TEST(test_cms_replication)
    RUN_TEST(test_no_replication)

    return ret;
}
