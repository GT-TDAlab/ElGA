/**
 * Test files for packing/unpacking
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "tests.hpp"

#include "address.hpp"
#include "types.hpp"
#include "pack.hpp"

using namespace elga;

int pack_agents() {
    ZMQAddress addr("4.5.4.5", 10);

    aid_t aid = 9876;

    char data[sizeof(msg_type_t)+sizeof(uint64_t)];
    char *data_ptr = data;

    pack_msg_agent(data_ptr, AGENT_JOIN, addr.serialize(), aid);

    uint64_t addr_ser;
    aid_t new_aid;
    unpack_agent(*(uint64_t*)&data[sizeof(msg_type_t)], addr_ser, new_aid);

    ASSERTEQ(addr_ser, addr.serialize());
    ASSERTEQ(new_aid, aid);

    return 0;
}

int pack_update() {
    edge_t e;
    e.src = 4444;
    e.dst = 5555;
    edge_type et = IN;
    update_t u;
    u.e = e;
    u.et = et;
    u.insert = true;

    char data[pack_msg_update_size];

    char *data_ptr = data;

    pack_msg_update(data_ptr, UPDATE_EDGE, u);

    update_t n_u;

    const char *new_data_ptr = &data[sizeof(msg_type_t)];

    unpack_update(new_data_ptr, n_u);

    ASSERTEQ(n_u.et, et);
    ASSERTEQNP(n_u.e, e);
    ASSERTEQ(n_u.insert, true);
    ASSERTEQNP(n_u, u);

    return 0;
}

int main(int argc, char **argv) {
    int ret = 0;

    RUN_TEST(pack_agents)
    RUN_TEST(pack_update)

    return ret;
}

