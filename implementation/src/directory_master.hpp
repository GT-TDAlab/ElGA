/**
 * ElGA directory master
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef DIRECTORY_MASTER_HPP
#define DIRECTORY_MASTER_HPP

#include "types.hpp"

#include <algorithm>
#include <vector>

#include "chatterbox.hpp"
#include "address.hpp"

namespace elga {

    namespace directory_master {

        /** Main entry point to run a directory master */
        int main(int argc, const char **argv, const ZMQAddress &directory, localnum_t ln);

    }

    /**
    * The Directory Master is the one non-elastic part of ElGA.
    * It is responsible for maintaining a list of directory servers and
    * providing a random directory server to any client.
    */
    class DirectoryMaster : public ZMQChatterbox {
        private:
            std::vector<uint64_t> directories_;
        public:
            DirectoryMaster(const ZMQAddress &addr) :
                ZMQChatterbox(addr), directories_() { }

            /** Begin the server */
            void start();

            /** Return all of the registered directory servers */
            void get_directories(zmq_socket_t sock);

            /** Find and return a random directory */
            void get_directory(zmq_socket_t sock);

            /** Support a directory joining */
            void dir_join(const char *data, size_t size);

            /** Support a directory leaving */
            void dir_leave(const char *data, size_t size);
    };


}

#endif
