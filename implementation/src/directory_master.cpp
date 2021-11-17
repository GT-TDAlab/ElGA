/**
 * ElGA directory master implementation
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "directory_master.hpp"

#include <random>
#include <thread>
#include <chrono>

#include <iostream>

using namespace elga;

namespace elga::directory_master {

    /** Helper function to print usage */
    void usage_() {
        std::cout << "Usage: directory-master [help]" << std::endl;
    }

    /** Helper function for printing out a help message */
    int help_() {
        std::cout << "\n"
            "The Directory Master service of ElGA.\n\n"
            "This should be run once per cluster.  It maintains a list of\n"
            "Directory servers that other components can connect to.\n\n"
            "Arguments:\n"
            "    help : display this message\n"
            ;
        return EXIT_SUCCESS;
    }


    int main(int argc, const char **argv, const ZMQAddress &directory, localnum_t ln) {
        if (argc > 1) { usage_(); return help_(); }

        if (ln != 0) throw std::runtime_error("Unimplemented.");

        // Create the directory master
        elga::DirectoryMaster dm(directory);
        dm.start();

        return 0;
    }

}

void DirectoryMaster::start() {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : DirectoryMaster] running" << std::endl;
    #endif

    bool keep_running = true;
    while (keep_running) {
        // Check for a global shutdown
        // If the directory master shuts down, the whole system should
        // stop
        if (global_shutdown) {
            #ifdef DEBUG_VERBOSE
            std::cerr << "[ElGA : DirectoryMaster] initiating shutdown" << std::endl;
            #endif
            msg_type_t shutdown = SHUTDOWN;
            pub((char*)&shutdown, sizeof(shutdown));

            keep_running = false;
            // Give a grace period to get the pub sent
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            break;
        }

        heartbeat();

        // Wait for a request
        #ifdef DEBUG_VERBOSE
        std::cerr << "[ElGA : DirectoryMaster] polling" << std::endl;
        #endif
        for (auto sock : poll()) {
            // We have no subscriptions, so any request is from a client

            ZMQMessage msg(sock);

            size_t total_size = msg.size();

            if (total_size < sizeof(msg_type_t)) throw std::runtime_error("Message too small");

            // Read out the message type and data
            const char *data = msg.data();

            #ifdef DEBUG_VERBOSE
            std::cerr << "[ElGA : DirectoryMaster] got query: " << (int)(*(const msg_type_t*)data) << std::endl;
            #endif

            switch (*(const msg_type_t*)data) {
                case GET_DIRECTORIES:
                    get_directories(sock);
                    break;
                case GET_DIRECTORY:
                    get_directory(sock);
                    break;
                case SHUTDOWN:
                    // Send an ack
                    ack(sock);
                    // Broadcast a shutdown
                    pub(data, total_size);
                    // Stop the server
                    keep_running = false;
                    break;
                case DIRECTORY_JOIN:
                    ack(sock);
                    dir_join(data, total_size);
                    break;
                case DIRECTORY_LEAVE:
                    ack(sock);
                    dir_leave(data, total_size);
                    break;
                #ifdef CONFIG_CS
                case CS_LB:
                #endif
                case UPDATE:
                case START:
                case SAVE:
                case DUMP:
                case RESET:
                case CHK_T:
                case VA:
                    ack(sock);
                    pub(data, total_size);
                    break;
                default:
                    throw std::runtime_error("Unknown message");
            }
        }
    }

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : DirectoryMaster] stopping" << std::endl;
    #endif
}

void DirectoryMaster::get_directories(zmq_socket_t sock) {
    std::cerr << "[Elga : DirectoryMaster] returning full directory list" << std::endl;

    // Build the output message buffer
    auto msg = ZMQMessage(sock, directories_.size()*sizeof(uint64_t));

    // Serialize and add each address into it
    uint64_t *raw_msg = (uint64_t*)msg.edit_data();
    size_t i = 0;
    for (const auto& addr : directories_)
        raw_msg[i++] = addr;

    msg.send();
    std::cerr << "[Elga : DirectoryMaster] sent" << std::endl;
}

void DirectoryMaster::get_directory(zmq_socket_t sock) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[Elga : DirectoryMaster] returning a random directory" << std::endl;
    #endif

    if (directories_.size() < 1) {
        ack(sock);
        return;
    }

    // Choose a random directory and return it
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<size_t> dist(0, directories_.size()-1);

    uint64_t dir = directories_[dist(mt)];
    send(sock, (char*)&dir, sizeof(dir));

    #ifdef DEBUG_VERBOSE
    std::cerr << "[Elga : DirectoryMaster] sent" << std::endl;
    #endif
}

void DirectoryMaster::dir_join(const char *data, size_t size) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[Elga : DirectoryMaster] received join request" << std::endl;
    #endif

    // Pull out the address
    if (size != sizeof(msg_type_t)+sizeof(uint64_t)) throw std::runtime_error("Message wrong size");
    uint64_t addr = *(uint64_t*)&data[1];

    // First, actually add the address
    directories_.push_back(addr);
    std::sort(directories_.begin(), directories_.end());

    // Then, broadcast it to all listeners
    pub(data, size);

    #ifdef DEBUG_VERBOSE
    std::cerr << "[Elga : DirectoryMaster] processed join request" << std::endl;
    #endif
}

void DirectoryMaster::dir_leave(const char *data, size_t size) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[Elga : DirectoryMaster] received leave request" << std::endl;
    #endif

    // Pull out the address
    if (size != sizeof(msg_type_t)+sizeof(uint64_t)) throw std::runtime_error("Message wrong size");
    uint64_t addr = *(uint64_t*)&data[1];

    // Find and remove the address
    auto range = std::equal_range(std::begin(directories_), std::end(directories_), addr);
    directories_.erase(range.first, range.second);

    // Broadcast the removal
    pub(data, size);

    #ifdef DEBUG_VERBOSE
    std::cerr << "[Elga : DirectoryMaster] processed leave request" << std::endl;
    #endif
}
