/**
 * ElGA client
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "client.hpp"

#include "pack.hpp"

#include <random>
#include <iostream>

using namespace elga;

namespace elga::client {

    /** Helper function to print usage */
    void usage_() {
        std::cout << "Usage: client query" << std::endl;
    }

    /** Helper function for printing out a help message */
    int help_() {
        std::cout << "\n"
            "The Client for ElGA.\n\n"
            "This is used to query various parts of ElGA, ranging from results to\n"
            "internal properties of various components.\n\n"
            "Query:\n"
            "    shutdown : shutdown the system gracefully\n"
            "    directories : query and return all directories\n"
            "    update : trigger entering the batch state without processing\n"
            #ifdef CONFIG_CS
            "    lb : trigger a load balancing\n"
            #endif
            #ifdef CONFIG_START_VTX
            "    start vtx : start the computation with vertex vtx\n"
            #else
            "    start : start the computation\n"
            #endif
            "    save : save the computation results to disk\n"
            "    dump : dump the current graph to disk\n"
            "    workload : query following workloads\n"
            "    query <vertex> : perform a vertex query\n"
            "    check-transpose : confirm the transpose\n"
            "    va : change virtual agent counts\n"
            "    help : display this message\n"
            ;
        return EXIT_SUCCESS;
    }


    int main(int argc, const char **argv, const ZMQAddress &directory_master, localnum_t ln) {
        if (argc < 2) { usage_(); return 1; }
        std::string query(argv[1]);

        if (ln != 0) throw std::runtime_error("Unimplemented.");

        if (query == "help") { usage_(); return help_(); }

        // Create the client
        elga::Client client(directory_master);

        // Now, perform the required client action
        if (query == "directories") {
            if (argc != 2) { usage_(); return help_(); }
            for (auto addr : client.query_directories()) {
                // Print out each
                std::cout << addr.get_remote_str() << std::endl;
            }
        } else if (query == "shutdown") {
            if (argc != 2) { usage_(); return help_(); }
            // Inform the directory to shutdown, which will propagate
            // through the system
            client.query(SHUTDOWN);
        } else if (query == "start") {
            #ifdef CONFIG_START_VTX
            if (argc != 3) { usage_(); return help_(); }
            client.start_vtx(std::strtoull(argv[2], NULL, 0));
            #else
            if (argc != 2) { usage_(); return help_(); }
            client.query(START);
            #endif
        } else if (query == "save") {
            if (argc != 2) { usage_(); return help_(); }
            client.query(SAVE);
        } else if (query == "dump") {
            if (argc != 2) { usage_(); return help_(); }
            client.query(DUMP);
        } else if (query == "update") {
            if (argc != 2) { usage_(); return help_(); }
            client.query(UPDATE);
        #ifdef CONFIG_CS
        } else if (query == "lb") {
            if (argc != 2) { usage_(); return help_(); }
            client.query(CS_LB);
        #endif
        } else if (query == "reset") {
            if (argc != 2) { usage_(); return help_(); }
            client.query(RESET);
        } else if (query == "workload") {
            if (argc != 2) { usage_(); return help_(); }
            client.workload();
        } else if (query == "check-transpose") {
            if (argc != 2) { usage_(); return help_(); }
            client.query(CHK_T);
        } else if (query == "va") {
            if (argc != 2) { usage_(); return help_(); }
            client.query(VA);
        } else if (query == "query") {
            if (argc != 3) { usage_(); return help_(); }
            client.query_vertex(std::strtoull(argv[2], NULL, 0));
        } else {
            throw arg_error("Unknown client command");
        }

        return 0;
    }

}

const std::vector<ZMQAddress> Client::query_directories() {
    std::vector<ZMQAddress> res;

    // Send a new message, asking the directory server
    dm_req_.send(GET_DIRECTORIES);
    ZMQMessage data = dm_req_.read();

    size_t total_size = data.size();
    const char *raw_msg = data.data();

    // Iterate through and create each directory address
    for (size_t pos = sizeof(uint64_t); pos <= total_size; pos += sizeof(uint64_t)) {
        size_t rpos = pos-sizeof(uint64_t);
        uint64_t ser_addr = *(uint64_t*)(&raw_msg[rpos]);
        res.push_back(ZMQAddress{ser_addr});
    }

    return res;
}

void Client::query(msg_type_t t) {
    dm_req_.send(t);
    dm_req_.wait_ack();
}

void Client::workload() {
    while (!ready_ && do_poll()) {
        if (global_shutdown) {
            std::cerr << "[ElGA : Client] shutting down" << std::endl;
            return;
        }
    }

    vertex_t max_v = 500000;

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<vertex_t> v_dist(0, max_v);

    size_t ct = 0;

    // Follow the rate schedule
    for (int block = 0; block <= 10 && !global_shutdown; ++block) {
        // Hold each block for five minutes
        std::cerr << "[ElGA : Client] workload block " << block << std::endl;
        time_t start = time(NULL);
        do {
            timer::Timer t;
            t.tick();
            size_t inner = 0;
            while (!global_shutdown && t.get_time().count() < 2.) {
                vertex_t v = v_dist(mt);
                query_vertex(v);
                ++ct;
                ++inner;
                if (block == 5)
                    usleep(6000);
                else
                    usleep(std::abs(5-block)*1000*10);
                t.tock();
            }
            printf("C,%d,%lu,%lu,%lf", block, time(NULL), ct, inner/t.get_time().count());
            std::cout << std::endl;
            while (do_poll(true)) {}
        } while (time(NULL)-start < 300 && !global_shutdown);
    }
}

void Client::query_vertex(vertex_t v) {
    while (!ready_ && do_poll()) {
        if (global_shutdown) {
            std::cerr << "[ElGA : Client] shutting down" << std::endl;
            return;
        }
    }
    // Find the agent for this vertex
    edge_t e;
    e.src = v;
    e.dst = -1;

    bool dummy;
    auto agent = find_agent(e, OUT, false, 0, dummy);

    ZMQRequester req { ZMQAddress(agent), addr_ };
    size_t msg_size = sizeof(msg_type_t)+sizeof(vertex_t);
    char msg[msg_size];
    char* msg_ptr = msg;
    pack_msg(msg_ptr, QUERY);
    pack_msg(msg_ptr, v);
    req.send(msg, msg_size);
    ZMQMessage resp = req.read();
}

void Client::handle_directory_update() {
    std::cerr << "[ElGA : Client] directory update" << std::endl;
}

#ifdef CONFIG_START_VTX
void Client::start_vtx(vertex_t start) {
    size_t msg_size = sizeof(msg_type_t)+sizeof(vertex_t);
    char msg[msg_size];
    char* msg_ptr = msg;
    pack_msg(msg_ptr, START);
    pack_msg(msg_ptr, start);
    dm_req_.send(msg, msg_size);
    dm_req_.wait_ack();
}
#endif
