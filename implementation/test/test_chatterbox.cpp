/**
 * Test files for the chatterbox
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

#include <string.h>

#include "chatterbox.hpp"

using namespace elga;

int test_sendrecv() {

    ZMQAddress r_addr("127.0.0.1", 0);
    ZMQAddress s_addr("127.0.0.1", 1);

    ZMQChatterbox r(r_addr);

    // Send a message
    ZMQRequester s(r_addr, s_addr);
    s.send(HEARTBEAT);

    // Ensure we receive it
    auto polled = r.poll();
    ASSERTEQ(polled.size(), 1);

    // Check the message
    ZMQMessage msg(polled[0]);
    ASSERTEQ(msg.size(), sizeof(msg_type_t));
    ASSERTEQ(*(msg_type_t*)msg.data(), HEARTBEAT);

    // Send an ack
    r.ack(polled[0]);
    // Ensure we get the ack
    s.wait_ack();

    return 0;
}

int test_pubsub() {

    ZMQAddress r_addr("127.0.0.1", 2);
    ZMQAddress s_addr("127.0.0.1", 3);

    ZMQChatterbox r(r_addr);
    ZMQChatterbox s(s_addr);

    // Subscribe to heartbeats
    r.sub(HEARTBEAT);
    // Listen to the sender
    r.sub_connect(s_addr);

    // Publish a heartbeat
    msg_type_t hb = HEARTBEAT;
    s.pub((char*)&hb, sizeof(hb));

    // Ensure we receive it
    auto polled = r.poll();
    ASSERTEQ(polled.size(), 1);

    // Check the message
    ZMQMessage msg(polled[0]);
    ASSERTEQ(msg.size(), sizeof(msg_type_t));
    ASSERTEQ(*(msg_type_t*)msg.data(), HEARTBEAT);

    return 0;
}

int test_pubnosub() {

    ZMQAddress r_addr("127.0.0.1", 4);
    ZMQAddress s_addr("127.0.0.1", 5);

    ZMQChatterbox r(r_addr);
    ZMQChatterbox s(s_addr);

    // Subscribe to heartbeats
    r.sub(HEARTBEAT);
    // Listen to the sender
    r.sub_connect(s_addr);

    // Publish a heartbeat
    msg_type_t hb = HEARTBEAT;
    s.pub((char*)&hb, sizeof(hb));

    // Ensure we receive it
    auto polled = r.poll();
    ASSERTEQ(polled.size(), 1);

    // Check the message
    ZMQMessage msg(polled[0]);
    ASSERTEQ(msg.size(), sizeof(msg_type_t));
    ASSERTEQ(*(msg_type_t*)msg.data(), HEARTBEAT);

    // Now disconnect
    r.sub_disconnect(s_addr);

    // Resend
    s.pub((char*)&hb, sizeof(hb));

    // Make sure we do not receive it
    // Ensure we receive it
    auto nextpoll = r.poll();
    ASSERTEQ(nextpoll.size(), 0);

    return 0;
}

int test_senddata() {

    ZMQAddress r_addr("127.0.0.1", 6);
    ZMQAddress s_addr("127.0.0.1", 7);

    ZMQChatterbox r(r_addr);

    // Send a message
    ZMQRequester s(r_addr, s_addr);
    char sdata[] = "OK";
    s.send(sdata, sizeof(sdata));

    // Ensure we receive it
    auto polled = r.poll();
    ASSERTEQ(polled.size(), 1);

    // Check the message
    ZMQMessage msg(polled[0]);
    ASSERTEQ(msg.size(), sizeof(sdata));
    ASSERTEQ(strcmp(msg.data(), sdata), 0);

    // Send back data
    char data[] = "testing";
    r.send(polled[0], data, sizeof(data));
    // Ensure we get the data
    ZMQMessage res = s.read();

    ASSERTEQ(strlen(res.data()), strlen(data));
    ASSERTEQ(strcmp(res.data(), data), 0);

    return 0;
}

int test_sendraw() {

    ZMQAddress r_addr("127.0.0.1", 8);
    ZMQAddress s_addr("127.0.0.1", 9);

    ZMQChatterbox r(r_addr);

    // Send a message
    ZMQRequester s(r_addr, s_addr);
    char sdata[] = "query";
    s.send(sdata, sizeof(sdata));

    // Ensure we receive it
    auto polled = r.poll();
    ASSERTEQ(polled.size(), 1);

    // Check the message
    ZMQMessage msg(polled[0]);
    ASSERTEQ(msg.size(), sizeof(sdata));
    ASSERTEQ(strcmp(msg.data(), sdata), 0);

    // Send back data in a raw fashion
    size_t size = 11;
    ZMQMessage outmsg(polled[0], size);
    char *raw_data = outmsg.edit_data();
    for (size_t ctr = 0; ctr < size-1; ++ctr)
        raw_data[ctr] = '0'+ctr;
    raw_data[size-1] = 0;
    outmsg.send();

    // Ensure we get the data
    ZMQMessage res = s.read();

    ASSERTEQ(strlen(res.data()), 10);
    ASSERTEQ(res.data()[2], '2');

    return 0;
}

int test_pushpull() {

    ZMQAddress pull_addr("127.0.0.1", 2);
    ZMQAddress push_addr("127.0.0.1", 3);

    ZMQChatterbox puller(pull_addr);

    ZMQRequester pusher(pull_addr, push_addr, PULL);

    // Send a message
    char smsg[sizeof(msg_type_t)+sizeof(uint64_t)];
    *(msg_type_t*)smsg = OUT_VN;
    uint64_t integer = 0x987654321abcdef;
    *(uint64_t*)&smsg[sizeof(msg_type_t)] = integer;

    pusher.send(smsg, sizeof(smsg));

    // Ensure we receive it
    auto polled = puller.poll();
    ASSERTEQ(polled.size(), 1);

    // Check the message
    ZMQMessage msg(polled[0]);
    ASSERTEQ(msg.size(), sizeof(smsg));
    ASSERTEQ(*(msg_type_t*)msg.data(), OUT_VN);
    ASSERTEQ(*(uint64_t*)(msg.data()+sizeof(msg_type_t)), integer);

    return 0;
}

int test_rempushpull() {

    ZMQAddress pull_addr("127.0.5.1", 0);
    ZMQAddress push_addr("127.0.6.1", 0);

    ZMQChatterbox puller(pull_addr);

    ZMQRequester pusher(pull_addr, push_addr, PULL);

    // Send a message
    char smsg[sizeof(msg_type_t)+sizeof(uint64_t)];
    *(msg_type_t*)smsg = OUT_VN;
    uint64_t integer = 0x987654321abcdee;
    *(uint64_t*)&smsg[sizeof(msg_type_t)] = integer;

    pusher.send(smsg, sizeof(smsg));

    // Ensure we receive it
    auto polled = puller.poll();
    ASSERTEQ(polled.size(), 1);

    // Check the message
    ZMQMessage msg(polled[0]);
    ASSERTEQ(msg.size(), sizeof(smsg));
    ASSERTEQ(*(msg_type_t*)msg.data(), OUT_VN);
    ASSERTEQ(*(uint64_t*)(msg.data()+sizeof(msg_type_t)), integer);

    return 0;
}

int main(int argc, char **argv) {
    int ret = 0;

    // Create a context
    ZMQChatterbox::Setup();

    try {

        RUN_TEST(test_sendrecv)
        RUN_TEST(test_pubsub)
        RUN_TEST(test_pubnosub)
        RUN_TEST(test_senddata)
        RUN_TEST(test_sendraw)
        RUN_TEST(test_pushpull)
        RUN_TEST(test_rempushpull)

    } catch(...) {
        ZMQChatterbox::Teardown();
        throw;
    }

    return ret;
}


