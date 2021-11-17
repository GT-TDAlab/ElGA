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

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "types.hpp"

#include <vector>

#include "participant.hpp"

namespace elga {

    namespace client {

        /** Main entry point to run a directory master */
        int main(int argc, const char **argv, const ZMQAddress &directory, localnum_t ln);

    }

    /**
     * The Client is able to query for results and other internal parts of
     * ElGA.  It is not intended to be long-living.
     */
    class Client : public Participant {
        private:
            ZMQRequester dm_req_;
        public:
            /** Construct the client, pointing to the given directory */
            Client(const ZMQAddress &dm) : Participant(ZMQAddress {}, dm, true),
                        dm_req_(dm, addr_) { }

            /** Query and return all directory servers */
            const std::vector<ZMQAddress> query_directories();

            /** Query with a single message type to the directory master */
            void query(msg_type_t t);

            /** Query for adding an agent */
            void agent_join(ZMQAddress &dir, ZMQAddress &agent, aid_t aid);

            /** Query following a workload pattern */
            void workload();

            /** Query a vertex */
            void query_vertex(vertex_t v);

            /** Report a directory update */
            void handle_directory_update();

            #ifdef CONFIG_START_VTX
            /** Start with a vertex */
            void start_vtx(vertex_t start);
            #endif
    };

}

#endif
