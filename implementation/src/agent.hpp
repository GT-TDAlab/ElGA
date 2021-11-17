/**
 * ElGA agent to manage the graph
 *
 * This command is responsible for running an agent, which is responsible
 * for both storing part of the graph and running local algorithms on that
 * part.
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef AGENT_HPP
#define AGENT_HPP

#include "timer.hpp"

#include "participant.hpp"
#include "algorithm.hpp"

#ifdef CONFIG_CS
#include "countminsketch.hpp"
#endif

#include <unordered_map>
#include <unordered_set>

#include "absl/container/flat_hash_set.h"
#include "absl/container/flat_hash_map.h"

#include <thread>
#include <mutex>
#include <iomanip>

namespace elga {

    extern std::mutex p_mutex;

    template<typename ...Args>
    inline __attribute__((always_inline))
    void info_agent_(uint64_t addr_ser, Args && ...args) {
        std::lock_guard<std::mutex> guard(p_mutex);

        std::cerr << "[ElGA : Agent : " << std::setw(2) << (addr_ser>>32) << "] ";
        (std::cerr << ... << args);
        std::cerr << std::endl;
    }

    template<typename ...Args>
    inline __attribute__((always_inline))
    void debug_agent_(uint64_t addr_ser, Args && ...args) {
        #ifdef DEBUG_VERBOSE
        std::lock_guard<std::mutex> guard(p_mutex);
        std::cerr << "[ElGA : Agent : " << std::setw(17) << addr_ser << " " << std::this_thread::get_id() << "] ";
        (std::cerr << ... << args);
        std::cerr << std::endl;
        #endif
    }


    namespace agent {

        /** Main entry point for the agent command */
        int main(int argc, const char **argv, const ZMQAddress &directory_master, localnum_t ln);

    }

    typedef enum agent_state {
        /** The initial state, where no processing will occur
         * and the graph statistics will never be updated.
         * In this state, edges are accepted but the reciprocal edges are
         * not created. */
        NO_PROCESS,
        /** The state of leaving no process, prior to starting */
        LEAVING_NO_PROCESS,
        /** The state where the batch computation has finished but a new
         * batch may or may not be ready.
         * We are idle, until an edge comes into the system somewhere. */
        IDLE,
        /** The state that closes the graph updates, brings them into the
         * graph itself, and requests corresponding out edges.
         * This state will then wait until the out edges are ready,
         * similar to LEAVING_NO_PROCESS, before moving into PROCESS. */
        FINALIZE_GRAPH_BATCH,
        /** The state where full processing is occurring, aggressively
         * trying to process all available data.
         * If neighbor messages enter while processing, it will continue
         * the processing looping */
        PROCESS,
        /** No vertices are expecting inputs from any more neighbors, and
         * so we are ready to join the upcoming barrier */
        JOIN_BARRIER,
        /** The state where we are at the barrier and are waiting for
         * receiving a sync from the directory, indicating everyone else
         * is at the barrier and we can move passed */
        WAIT_FOR_SYNC,
        /** A waiting state for a forced sync */
        WAIT_FOR_LB,
        WAIT_FOR_LB_SYNC,
        /** The state where the agent is waiting for edge movements */
        WAIT_EDGE_MOVE
    } agent_state_t;

    /**
     * Main graph agent, which holds part of the graph in memory and
     * executes algorithms.
     */
    class Agent : public Participant {
        private:
            /** Holds the assigned portions of the graph */
            absl::flat_hash_map<vertex_t, VertexStorage> graph_;

            /** Hold the local storage from all known vertices */
            vn_t vn_;
            vnw_t vn_wait_;
            size_t vn_count_;
            vnr_t vn_remaining_;

            /** Keep track of the number of vertices and edges */
            size_t nV_;
            size_t nE_;
            size_t global_nV_;
            size_t global_nE_;

            /** Hold whether we need to update nV globally */
            double update_nV_;
            absl::flat_hash_set<vertex_t> update_nV_set_;
            /** Hold whether we need to update nE globally */
            int64_t update_nE_;

            /** Support calculating runtime */
            timer::Timer batch_timer_;
            timer::Timer update_timer_;
            timer::Timer ss_timer_;

            /** The algorithm to run */
            Algorithm alg_;

            /** The current agent state */
            agent_state_t state_;

            /** Keep track of active vertices */
            absl::flat_hash_set<vertex_t> active_;

            /** Keep track of the number of dormant vertices */
            absl::flat_hash_set<vertex_t> dormant_;

            size_t num_dormant_;
            size_t num_inactive_;
            #if defined(CONFIG_BSP) || defined(CONFIG_LBSP)
            absl::flat_hash_map<it_t, int> agent_msgs_needed_;
            absl::flat_hash_map<uint64_t, std::vector<VertexNotification> > out_vn_msgs;
            it_t it_;
            #endif

            /** Contains the number of virtual agents */
            aid_t vagent_count_;

            /** Hold incoming graph changes for the next batch */
            absl::flat_hash_set<update_t> update_set_;

            /** Keep track of whether we asked to leave idle */
            bool requested_leave_idle_;

            /** Keep track of the batch index we are on */
            batch_t batch_;

            /** Keep track of the number of update acks that are needed */
            int32_t update_acks_needed_;

            #ifdef DUMP_MSG_DIST
            /** Keep track of the number of times messages have been saved */
            size_t dump_msg_dist_count;
            #endif

            #ifdef CONFIG_CS
            /** Keep an agent-specific sketch */
            CountMinSketch cms;
            bool push_sketch_;
            #endif

            #ifdef CONFIG_TIME_INGESTION
            timer::Timer ingest_t_;
            size_t last_edges_;
            #endif

            /** Maintain updates to move */
            absl::flat_hash_map<uint64_t, std::vector<update_t>> moves;

            /** Keep the serialized address for debugging */
            uint64_t addr_ser;

            /** Get the owner of the given update */
            uint64_t get_owner(update_t& u);

            #ifdef CONFIG_LBSP
            absl::flat_hash_map<vertex_t, std::vector<vertex_t>> tmap;
            #ifdef CONFIG_TACTIVATE
            absl::flat_hash_map<it_t, absl::flat_hash_set<vertex_t>> tactivate;
            #endif
            #endif

            // Measure edge movement time
            timer::Timer move_timer_;

            #ifdef CONFIG_AUTOSCALE
            /** Keep track of query rates */
            timer::Timer query_rate_t_;
            double query_rate_;
            size_t query_count_;
            bool dying;
            bool dead;
            #endif
        public:
            Agent(const ZMQAddress &addr, const ZMQAddress &directory_master) :
                Participant(addr, directory_master, true),
                graph_(), vn_(), vn_wait_(), vn_count_(0), vn_remaining_(),
                nV_(0), nE_(0), global_nV_(0), global_nE_(0),
                update_nV_(0), update_nE_(0),
                batch_timer_("batch"),
                update_timer_("update"),
                ss_timer_("superstep"),
                alg_(),
                state_(NO_PROCESS),
                active_(),
                dormant_(),
                num_dormant_(0),
                num_inactive_(0),
                #if defined(CONFIG_BSP) || defined(CONFIG_LBSP)
                it_(-1),
                #endif
                vagent_count_(STARTING_VAGENTS),
                update_set_(),
                requested_leave_idle_(false),
                batch_(0),
                update_acks_needed_(0),
                #ifdef DUMP_MSG_DIST
                dump_msg_dist_count(0),
                #endif
                #ifdef CONFIG_CS
                push_sketch_(false),
                #endif
                #ifdef CONFIG_TIME_INGESTION
                last_edges_(0),
                #endif
                addr_ser(addr_.serialize()),
                move_timer_("edgemove")
                #ifdef CONFIG_AUTOSCALE
                ,query_rate_t_("queryrate"),
                query_rate_(0.0),
                query_count_(0),
                dying(false),
                dead(false)
                #endif
                { }

            /** Register ourselves with the directory */
            void register_dir();

            /** Handle an edge change */
            void change_edge(update_t u, bool count_deg=true);

            /** Process the vertex notifications */
            void process_vn(const char *data, size_t size);

            /** Handle directory updates */
            void handle_directory_update();

            /** Handle agent-specific messages */
            bool handle_msg(zmq_socket_t sock, msg_type_t t, const char *data, size_t size);

            /** Agent-specific pre-polling to run algorithms */
            void pre_poll();

            /** Handle any heartbeat-timed work */
            bool heartbeat();

            /** Write out the current algorithm results to disk */
            void save();

            /** Write the current graph out to disk */
            void dump();

            /** Process all vertices, sending out updates */
            void process_vertices();

            /** Begin the process of leaving the IDLE state */
            void start_leaving_idle();

            /** Send all OUT edges, prior to starting computation */
            void send_out_edges(bool check=false);

            /** Inform the directory we are done waiting for out edges */
            void done_waiting_ready_nv_ne();

            /** Move dormant items to active */
            void move_dormant_active();

            /** Move edges that do not belong */
            void send_move_edges();

            /** Process the graph update queue and create updates to form
             * corresponding edges */
            void finalize_graph_batch();

            /** Clear our any memory used specific to a batch */
            void clear_batch_mem();

            /** Perform per-iteration garbage collection */
            void gc();

            /** Handle agent-specific shutdowns */
            bool shutdown();

            #ifdef CONFIG_CS
            /** Send the sketch up to the directory */
            void push_sketch();
            #endif

            #ifdef CONFIG_AUTOSCALE
            /** Keep track of query rates */
            void track_query_rate();
            #endif

            /** Balance virtual agent counts */
            void balance_va();
    };

}

#endif
