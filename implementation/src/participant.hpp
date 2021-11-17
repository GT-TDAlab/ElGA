/**
 * ElGA participant in the graph
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef PARTICIPANT_HPP
#define PARTICIPANT_HPP

#include "types.hpp"
#include "timer.hpp"
#include "address.hpp"
#include "chatterbox.hpp"
#include "consistenthasher.hpp"

#include <tuple>
#include "absl/container/flat_hash_map.h"
#include <list>

namespace elga {

    /**
     * A Participant is a node that will connect to the directory and
     * determine how to access the graph, using consistent hashing and
     * replication maps.
     * Agents, Clients, and Streamers are all Paricipants
     */
    class Participant : public ZMQChatterbox {
        private:
            /** Keep track of our current directory server */
            ZMQAddress directory_;

            /** Keep track of the directory master, if directory leaves */
            const ZMQAddress dm_;

            #ifdef CONFIG_USE_AGENT_CACHE
            /** Keep a cache of resolved agent lookups */
            absl::flat_hash_map<std::tuple<edge_t, edge_type, bool>, std::tuple<uint64_t, bool> > agent_cache_;
            #endif

            /** Keep an LRU cache with open TCP connections */
            typedef std::list<ZMQRequester> l_req;
            l_req lru_;
            absl::flat_hash_map<uint64_t, l_req::iterator> lru_lookup_;

            /** Keep track of agents */
            std::vector<uint64_t> agents_;
        protected:
            /** Keep track of replication */
            #ifdef CONFIG_CS
            CMSReplicationMap rm_;
            #else
            NoReplication rm_;
            #endif

            /** Keep track of real / non-virtual agents */
            std::vector<uint64_t> real_agents_;

            /** This indicates whether we can support any queries */
            bool ready_;

            /** Support consistent hashing for identifying agents */
            ConsistentHasher ch_;

            /** Keep an open connection to the directory */
            ZMQRequester d_req_;

            /** Store the number of agents and virtual agents */
            size_t num_agents_;
            size_t num_vagents_;

            /** Keep track of whether we are doing actual work */
            bool working_;

            #ifdef CONFIG_TIME_FIND_AGENTS
            /** Keep track of agent search time */
            timer::Timer find_agent_t;
            #endif
        public:
            /** Create a participant at addr using the directory master */
            Participant(ZMQAddress addr, const ZMQAddress &dm, bool persist=false);

            /** Begin the participant loop */
            void start();

            /** Perform any local shutdown actions */
            virtual bool shutdown() { return false; }

            /** Support custom loop actions */
            virtual bool handle_msg(zmq_socket_t sock, msg_type_t t, const char *data, size_t size) { return false; }

            /** Support custom actions after a directory update */
            virtual void handle_directory_update() { }

            /** Handle a directory update */
            void directory_update(const char *data, size_t size);

            /** Handle participant specific actions before a general poll */
            virtual void pre_poll() { }

            /** Perform a poll loop, checking for participant messages.
             * If drain=true, then it will return false when there are no
             * more messages and will not block.
             * If drain=false, then it will return false if the loop
             * should terminate and will block for messages.
             */
            bool do_poll(bool drain=false);

            /** Perform a heartbeat */
            virtual bool heartbeat() { return true; }

            /** Count the number of replicas for a vertex */
            int32_t count_agent_reps(vertex_t v) {
                return ch_.count_reps(v)-1;
            }

            /** Find the destination agent for a given edge
             *
             * Parameters:
             *  e : the edge to find an agent for
             *  et : the edge type, either an IN or OUT edge
             *  find_owner : if true, this will find the exact owner of
             *      the edge and return it.
             *      If false, it will find any owner of the target vertex,
             *      not necessarily the replication which owns that
             *      particular edge.
             *  owner_check : <only if not find_owner> will specify
             *      which owner (serialized agent) to check to see if it
             *      also owns the target vertex.
             *      If set to 0, such an owner check will not be
             *      performed.
             *  have_ownership : an out variable. If find_owner is false,
             *      it will contain true only if the owner check was
             *      performed and the the given owner was found to be a
             *      target vertex owner.
             *      Otherwise, if find_owner is true, it will be set to
             *      false.
             *
             *  Returns: the serialized agent address to connect to for
             *  the edge query
             */
            uint64_t find_agent(edge_t e, edge_type et, bool find_owner, uint64_t owner_check, bool &have_ownership, bool return_va=false);

            /** Find and return the requester from the LRU
             *
             * Parameters:
             *  agent_ser : the agent to retrieve a requester for
             *  use_buffering : if true, use default buffering. If false,
             *      then do not use buffering, that is set the water mark
             *      to 1 */
            ZMQRequester & get_requester(uint64_t agent_ser, bool use_buffering=true);
    };

}

#endif
