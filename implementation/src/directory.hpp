/**
 * ElGA directory server
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef DIRECTORY_HPP
#define DIRECTORY_HPP

#include "types.hpp"

#include <vector>
#include <tuple>
#include "absl/container/flat_hash_set.h"
#include "absl/container/flat_hash_map.h"

#include "chatterbox.hpp"
#include "address.hpp"

#include "countminsketch.hpp"

namespace elga {

    namespace directory {

        /** Main entry point to run a directory master */
        int main(int argc, const char **argv, const ZMQAddress &directory_master, localnum_t ln);

    }

    /**
     * Each Directory server is responsible for holding the active list of
     * agents and maintaining the list.
     * They will synchronize with each other on updates.
     * These servers are the entry point for clients, streamers, and
     * agents to determine how to send information.
     */
    class Directory : public ZMQChatterbox {
        private:
            /** Keep track of the agents registered with the directory */
            absl::flat_hash_set<uint64_t> agents_;

            /** Keep track of the directory master address */
            const ZMQAddress dm_;

            /** Keep track of other directory servers */
            absl::flat_hash_set<uint64_t> directories_;

            /** Keep track of whether to send an update */
            bool notify_;
            bool notify_changed_;

            /** Keep track of the graph statistics */
            double nV_;
            size_t nE_;
            #ifdef CONFIG_CS
            CountMinSketch cms_;
            size_t cms_recv_;
            #endif
            size_t simple_sync_;

            /** Keep a counter for how many agents have indicated sync */
            absl::flat_hash_map<batch_t, absl::flat_hash_map<it_t, size_t>> sync_ctr_;
            absl::flat_hash_map<batch_t, absl::flat_hash_map<it_t, size_t>> num_dormant_;
            size_t ready_ctr_;

            /** Keep track of the current batch */
            it_t it_;
            batch_t batch_;

            /** Keep track of whether the agents are in an idle state */
            bool agents_idle_;

            /** Keep our serialized address for printing */
            uint64_t addr_ser;

            #ifdef CONFIG_AUTOSCALE
            absl::flat_hash_map<uint64_t, double> as_rate_;
            int as_wait_;
            absl::flat_hash_set<uint64_t> dead_agents_;
            size_t as_req_;
            #endif
        public:
            Directory(const ZMQAddress &addr, const ZMQAddress &directory_master) :
                    ZMQChatterbox(addr), agents_(),
                    dm_(directory_master), directories_(), notify_(false), notify_changed_(false),
                    nV_(0), nE_(0),
                    #ifdef CONFIG_CS
                    cms_recv_(0),
                    #endif
                    simple_sync_(0),
                    ready_ctr_(0),
                    it_(0),
                    batch_(0), agents_idle_(false),
                    addr_ser(addr_.serialize())
                    #ifdef CONFIG_AUTOSCALE
                    ,as_wait_(AUTOSCALE_EMA),as_req_(0)
                    #endif
                    { }

            /** Join directory */
            void join_directory();

            /** Join peers */
            void join_peers();

            /** Join a specific peer */
            void join_peer(uint64_t ser_addr);

            /** Leave a specific peer */
            void leave_peer(uint64_t ser_addr);

            /** Handle agents joining */
            bool agent_join(uint64_t *agent_list, size_t num_agents);

            /** Handle agents leaving */
            bool agent_leave(uint64_t *agent_list, size_t num_agents);

            #ifdef CONFIG_CS
            /** Handle a sketch update from an agent */
            void cs_update(const char *data, size_t cs_size);
            #endif

            /** Process a clean shutdown */
            void shutdown();

            /** Begin the server */
            void start();

            /** Process a heartbeat */
            bool heartbeat();

            /** Handle autoscaling */
            void autoscaler();
    };


}

#endif
