/**
 * ElGA agent implementation
 *
 * Authors: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "agent.hpp"

#include "pack.hpp"

#include <fstream>
#include <sstream>

#include <iomanip>
#include <thread>
#include <mutex>

#include "absl/container/flat_hash_map.h"

using namespace elga;

#ifdef CONFIG_BSP

void Agent::process_vertices() {
    // Only process when all vertices are active
    if (agent_msgs_needed_[it_+1] > 0) {
        debug_agent_(addr_ser, "[", it_+1, "] WAITING ON ", agent_msgs_needed_[it_+1]);
        return;
    }

    bool vote_stop = true;

    #ifdef CONFIG_CS
    absl::flat_hash_map<uint64_t, std::vector<std::tuple<it_t, vertex_t, ReplicaLocalStorage&> > > out_rep_msgs;
    #endif

    auto proc_block = [&](const vertex_t & v, VertexStorage &gv) {
        debug_agent_(addr_ser, "PRC VTX | ", gv.vertex);

        // Prepare its notification space
        vnw_t local_vn_wait;
        VertexNotification vertex_notification {};
        bool notify_out = false;
        bool notify_in = false;
        bool notify_replica = false;

        // Force the state to active
        gv.local.state = ACTIVE;

        // Run it
        alg_.run(gv, global_nV_, vn_, local_vn_wait, vn_remaining_,
                vertex_notification, notify_out, notify_in,
                notify_replica);

        if (gv.local.state == DORMANT)
            ++num_dormant_;
        if (gv.local.state == INACTIVE)
            ++num_inactive_;

        // Inspect its state
        if (notify_out || notify_in) {
            debug_agent_(addr_ser, "NOTIFY |");
            vertex_notification.v = v;

            #ifndef CONFIG_NOTIFY_AGG
            absl::flat_hash_set<uint64_t> notify_agents;
            #endif

            // Send out a vertex update to each neighbor
            if (notify_out) {
                for (const auto &n : gv.out_neighbors) {
                    bool dummy;
                    edge_t e;
                    e.src = v;
                    e.dst = n;
                    uint64_t agent_dst = find_agent(e, IN, true, 0, dummy);
                    if (agent_dst == addr_ser) {
                        // We can directly add this
                        vn_[gv.local.iteration][v] = vertex_notification;
                        continue;
                    }
                    #ifndef CONFIG_NOTIFY_AGG
                    notify_agents.insert(agent_dst);
                    #else
                    vertex_notification.n = n;
                    out_vn_msgs[agent_dst].push_back(vertex_notification);
                    #endif
                }
            }
            if (notify_in) {
                for (const auto &n : gv.in_neighbors) {
                    bool dummy;
                    edge_t e;
                    e.src = n;
                    e.dst = v;
                    uint64_t agent_dst = find_agent(e, OUT, true, 0, dummy);
                    if (agent_dst == addr_ser) {
                        // We can directly add this
                        vn_[gv.local.iteration][v] = vertex_notification;
                        continue;
                    }
                    #ifndef CONFIG_NOTIFY_AGG
                    notify_agents.insert(agent_dst);
                    #else
                    vertex_notification.n = n;
                    out_vn_msgs[agent_dst].push_back(vertex_notification);
                    #endif
                }
            }
            #ifndef CONFIG_NOTIFY_AGG
            for (const auto & agent_dst : notify_agents) {
                out_vn_msgs[agent_dst].push_back(vertex_notification);
            }
            #endif
        }
        #ifdef CONFIG_CS
        if (notify_replica) {
            debug_agent_(addr_ser, "NTFY R  | ", v);
            // Send the replica state to each necessary other agent
            it_t it = gv.local.iteration;
            ReplicaLocalStorage & rs = gv.replica_storage[it][gv.self];
            for (uint64_t rep_agent : gv.replicas) {
                if (rep_agent == addr_ser) continue;
                out_rep_msgs[rep_agent].push_back({it, v, rs});
            }
        }
        #endif

        if (gv.local.state != INACTIVE)
            vote_stop = false;
    };

    #ifdef CONFIG_CS
    // Find any replicas; if so, wait to process remaining
    bool cont = true;
    for (auto & [v, gv] : graph_) {
        auto v_it = gv.local.iteration;
        if (gv.replicas.size() == 0) continue;
        debug_agent_(addr_ser, " proc-pre: ", v, " rep=", gv.replica_storage[v_it].size(), "/", gv.replicas.size(), " count=", count_agent_reps(v), " self=", gv.replica_storage[v_it].count(gv.self));
        if (gv.replicas.size() == gv.replica_storage[v_it].size()) {
            if (gv.local.state == REPWAIT)
                // This was waiting, but now has enough to continue
                gv.local.state = ACTIVE;
            debug_agent_(addr_ser, "has enough, not processing: ", v);
            continue;
        } else if (gv.replica_storage[v_it].count(gv.self) > 0) {
            // We have some, but not all; continue waiting
            debug_agent_(addr_ser, "has some, not enough, not continuing: ", v);
            cont = false;
            continue;
        }
        // This does not have enough; if it is already waiting, then continue
        // waiting
        if (gv.local.state == REPWAIT) {
            cont = false;
            continue;
        }

        // Process to send out replicas as appropriate
        debug_agent_(addr_ser, "proc_block v=", v);
        proc_block(v, gv);

        // If it is not waiting, then continue
        if (gv.local.state != REPWAIT) continue;

        if (gv.replicas.size() == gv.replica_storage[v_it].size()) {
            // This was waiting, but now has enough to continue
            gv.local.state = ACTIVE;
            continue;
        }
        // Otherwise, we need to wait on it
        cont = false;
    }
    // Send out replica messages
    for (auto & [out_agent, reps] : out_rep_msgs) {
        size_t num = reps.size();
        size_t msg_size = sizeof(msg_type_t)+num*(sizeof(ReplicaLocalStorage)+sizeof(vertex_t)+sizeof(it_t))+sizeof(uint64_t);
        ZMQRequester &req = get_requester(out_agent);
        char *msg = new char[msg_size];
        char *msg_ptr = msg;
        pack_msg(msg_ptr, RV);
        pack_single(msg_ptr, addr_ser);
        for (auto & [it, v, rep] : reps) {
            pack_single(msg_ptr, it);
            pack_single(msg_ptr, v);
            pack_single(msg_ptr, rep);
        }
        req.send(msg, msg_size);
        delete [] msg;
    }
    if (!cont) {
        debug_agent_(addr_ser, "not continuing");
        return;
    }
    #endif

    it_t it = ++it_;

    debug_agent_(addr_ser, "PROCESS | ", it);

    for (auto & [v, gv] : graph_) {
        proc_block(v, gv);
    }

    // Now, send out the neighbor, replica, and vertex messages
    #ifdef DUMP_MSG_DIST
    std::stringstream ofn;

    ofn << SAVE_DIR << "/dist." << addr_ser << ".txt";

    if (std::ofstream of {ofn.str(), ios_base::app}) {
        for (const auto& [agent_dst, vn_msgs] : out_vn_msgs) {
            of << dump_msg_dist_count << " " << agent_dst << " " << vn_msgs.size() << '\n';
        }
        ++dump_msg_dist_count;
    } else
        throw std::runtime_error("Error opening output file");
    #endif

    for (auto& [agent_dst, vn_msgs] : out_vn_msgs) {
        // We want to send the list of vn msgs to the given agent
        size_t vn_msgs_size = vn_msgs.size();
        size_t msg_size = sizeof(msg_type_t)+sizeof(it_t)+sizeof(VertexNotification)*vn_msgs_size;
        ZMQRequester &req = get_requester(agent_dst);
        #ifdef CONFIG_PREPARE_SEND
        auto msg = req.prepare_send(msg_size);
        char *msg_ptr = msg.edit_data();
        #else
        char *msg = new char[msg_size];
        char *msg_ptr = msg;
        #endif

        pack_msg(msg_ptr, OUT_VN);
        pack_single(msg_ptr, (it_t)(it+1));
        for (VertexNotification &vn : vn_msgs)
            pack_single(msg_ptr, vn);

        #ifdef CONFIG_PREPARE_SEND
        msg.send();
        #else
        req.send(msg, msg_size);
        delete [] msg;
        #endif

        vn_msgs.clear();
    }

    // Count the number of agents we expect messages from
    absl::flat_hash_set<uint64_t> agents_used;
    for (const auto & [dst, gv] : graph_) {
        for (const auto & src : gv.in_neighbors) {
            bool dummy;
            edge_t e;
            e.src = src;
            e.dst = dst;
            uint64_t agent = find_agent(e, OUT, true, 0, dummy);
            if (agent != addr_ser)
                agents_used.insert(agent);
        }
    }
    agent_msgs_needed_[it+1] += agents_used.size();
    debug_agent_(addr_ser, "NEED ", agent_msgs_needed_[it+1]);

    // Increase the iteration counter
    ++it;
    while (it >= vn_count_-1) {
        vn_.push_back({});
        vn_wait_.push_back({});
        vn_remaining_.push_back(0);
        ++vn_count_;
    }

    if (vote_stop) {
        num_dormant_ = 0;
        num_inactive_ = graph_.size();
    } else {
        num_dormant_ = graph_.size();
        num_inactive_ = 0;
    }

    if (agent_msgs_needed_[it_+1] == 0) {
        debug_agent_(addr_ser, "JOIN BARRIER");
        state_ = JOIN_BARRIER;
    }

    pre_poll();
}

#endif
