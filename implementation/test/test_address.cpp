/**
 * Test files for the chatterbox addresses
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

using namespace elga;

int test_address_parsing() {
    // Create a valid IP address
    ZMQAddress a("1.2.3.4", 0);
    // Ensure the IP is extracted correctly
    ASSERTEQ(a.get_addr(), 67305985);

    // Make sure a bad IP fails
    try {
        ZMQAddress a("localhost", 0);
        ASSERTEQ(1,0);
    } catch(...) {
        ASSERTEQ(1,1);
    }

    ZMQAddress b("10.0xff", 0);
    ASSERTEQ(b.get_addr(), 4278190090);

    return 0;
}

int test_address_serialization() {
    ZMQAddress a("1.2.3.4", 15);
    auto as = a.serialize();

    ZMQAddress b(as);
    ASSERTEQ(b.get_addr(), a.get_addr());
    ASSERTEQ(b.get_localnum(), a.get_localnum());

    return 0;
}

int test_address_str() {
    ZMQAddress a("1.2.3.4", 15);

    ASSERTEQ(std::string(a.get_remote_str()), "tcp://1.2.3.4:17215");
    ASSERTEQ(std::string(a.get_local_str()), "inproc://15");

    return 0;
}

int test_address_remlocal() {
    ZMQAddress a("1.2.3.4", 15);
    ZMQAddress b("1.2.3.4", 16);
    ZMQAddress c("1.2.3.5", 17);

    ASSERTEQ(std::string(b.get_conn_str(a, REQUEST)), "inproc://16");
    ASSERTEQ(std::string(b.get_conn_str(a, PUBLISH)), "inproc://116");

    ASSERTEQ(std::string(b.get_conn_str(c, REQUEST)), "tcp://1.2.3.4:17216");
    ASSERTEQ(std::string(b.get_conn_str(c, PUBLISH)), "tcp://1.2.3.4:17316");

    return 0;
}

int test_address_pubstr() {
    ZMQAddress a("99.99.99.98", 99);

    ASSERTEQ(std::string(a.get_remote_pub_str()), "tcp://99.99.99.98:17399");
    ASSERTEQ(std::string(a.get_local_pub_str()), "inproc://199");

    return 0;
}

int test_address_pullstr() {
    ZMQAddress a("99.99.99.98", 99);

    ASSERTEQ(std::string(a.get_remote_pull_str()), "tcp://99.99.99.98:17499");
    ASSERTEQ(std::string(a.get_local_pull_str()), "inproc://299");

    return 0;
}

int test_zero() {
    ZMQAddress a("4.3.5.4", 0);
    ZMQAddress z{};

    ZMQAddress b("0.0.0.0", 10);

    ASSERTEQ(a.is_zero(), false);
    ASSERTEQ(z.is_zero(), true);
    ASSERTEQ(b.is_zero(), true);

    return 0;
}

int test_emptyzero() {
    ZMQAddress z{};

    ASSERTEQ(std::string(z.get_conn_str(z, REQUEST)), "");

    return 0;
}

int main(int argc, char **argv) {
    int ret = 0;

    RUN_TEST(test_address_parsing)
    RUN_TEST(test_address_serialization)
    RUN_TEST(test_address_str)
    RUN_TEST(test_address_remlocal)
    RUN_TEST(test_address_pubstr)
    RUN_TEST(test_address_pullstr)
    RUN_TEST(test_zero)
    RUN_TEST(test_emptyzero);

    return ret;
}

