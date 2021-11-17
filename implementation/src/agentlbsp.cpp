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

#ifdef CONFIG_LBSP

void Agent::process_vertices() {
    // Only process when all vertices are active
    if (agent_msgs_needed_[it_+1] > 0) {
        debug_agent_(addr_ser, "[", it_+1, "] WAITING ON ", agent_msgs_needed_[it_+1]);
        return;
    }

    // BSP processing means we will process each vertex
    uint64_t my_agent_ser = addr_ser;

    bool vote_stop = true;

    vnw_t local_vn_wait;

    num_inactive_ = 0;
    num_dormant_ = 0;

    #ifdef CONFIG_CS
    absl::flat_hash_map<uint64_t, std::vector<std::tuple<it_t, vertex_t, ReplicaLocalStorage&> > > out_rep_msgs;
    #endif

    #ifdef CONFIG_TACTIVATE
    debug_agent_(addr_ser, "tactivate ", it, " = ", tactivate[it].size());
    #endif

    auto proc_block = [&](const vertex_t & v, VertexStorage &gv) {
        debug_agent_(addr_ser, "PRC VTX | ", gv.vertex);

        // Prepare its notification space
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
            vertex_notification.v = v;

            absl::flat_hash_set<uint64_t> notify_agents;

            if (notify_out) {
                // Send out a vertex update to each neighbor
                for (const auto &n : gv.out_neighbors) {
                    bool dummy;
                    edge_t e;
                    e.src = v;
                    e.dst = n;
                    uint64_t agent_dst = find_agent(e, IN, true, 0, dummy);
                    if (agent_dst == addr_ser) {
                        // We can directly add this
                        #ifdef CONFIG_TACTIVATE
                        if (graph_[n].local.new_cc > vertex_notification.cc) {
                            graph_[n].local.new_cc = vertex_notification.cc;
                            tactivate[it+1].insert(n);
                        }
                        #else
                        alg_.set_active(graph_[n], vertex_notification);
                        #endif
                        continue;
                    }
                    notify_agents.insert(agent_dst);
                }
            }
            #ifndef CONFIG_TACTIVATE
            vn_[v] = vertex_notification;
            #endif
            if (notify_in) {
                for (const auto &n : gv.in_neighbors) {
                    bool dummy;
                    edge_t e;
                    e.src = n;
                    e.dst = v;
                    uint64_t agent_dst = find_agent(e, OUT, true, 0, dummy);
                    if (agent_dst == addr_ser) {
                        // We can directly add this
                        #ifdef CONFIG_TACTIVATE
                        if (graph_[n].local.new_cc > vertex_notification.cc) {
                            graph_[n].local.new_cc = vertex_notification.cc;
                            tactivate[it+1].insert(n);
                        }
                        #else
                        alg_.set_active(graph_[n], vertex_notification);
                        #endif
                        continue;
                    }
                    notify_agents.insert(agent_dst);
                }
            }
            vote_stop = false;

            for (const auto & agent_dst : notify_agents) {
                out_vn_msgs[agent_dst].push_back(vertex_notification);
            }
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
    };
    #ifdef CONFIG_CS
    // Find any replicas; if so, wait to process remaining
    auto send_out_rep = [&]() {
        // Send out replica messages
        debug_agent_(addr_ser, "sending ", out_rep_msgs.size());
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
        out_rep_msgs.clear();
    };
    if (!alg_.skip_rep_wait()) {
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
        send_out_rep();
        if (!cont) {
            debug_agent_(addr_ser, "not continuing");
            return;
        }
    }
    #endif

    it_t it = ++it_;

    local_vn_wait.resize(it+2);

    debug_agent_(addr_ser, "PROCESS | ", it);

    #if defined(CONFIG_TACTIVATE)
    if (it == 0) {
        for (auto & [v, gv] : graph_) {
            proc_block(v, gv);
        }
    } else {
        for (vertex_t v : tactivate[it]) {
            auto & gv = graph_[v];
            proc_block(v, gv);
        }
    }
    #else
    for (auto & [v, gv] : graph_) {
        proc_block(v, gv);
    }
    #endif
    #ifdef CONFIG_CS
    send_out_rep();
    #endif

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
    for (const auto &agent_dst : real_agents_) {
        if (agent_dst == my_agent_ser) continue;
        if (out_vn_msgs.count(agent_dst) > 0) {
            auto& vn_msgs = out_vn_msgs[agent_dst];

            size_t vn_msgs_size = vn_msgs.size();
            size_t msg_size = sizeof(msg_type_t)+sizeof(it)+sizeof(VertexNotification)*vn_msgs_size;
            ZMQRequester &req = get_requester(agent_dst);
            #ifdef CONFIG_PREPARE_SEND
            auto msg = req.prepare_send(msg_size);
            char *msg_ptr = msg.edit_data();
            #else
            char *msg = new char[msg_size];
            char *msg_ptr = msg;
            #endif

            pack_msg(msg_ptr, OUT_VN);
            pack_single(msg_ptr, it+1);
            for (const VertexNotification &vn : vn_msgs)
                pack_single(msg_ptr, vn);

            #ifdef CONFIG_PREPARE_SEND
            msg.send();
            #else
            req.send(msg, msg_size);
            delete [] msg;
            #endif

            vn_msgs.clear();
            vn_msgs.reserve(vn_msgs_size);
        } else {
            // Send an empty message
            size_t msg_size = sizeof(msg_type_t)+sizeof(it);
            ZMQRequester &req = get_requester(agent_dst);
            char *msg = new char[msg_size];
            char *msg_ptr = msg;

            pack_msg(msg_ptr, OUT_VN);
            pack_single(msg_ptr, it+1);

            req.send(msg, msg_size);
            delete [] msg;
        }
    }

    // Count the number of agents we expect messages from
    agent_msgs_needed_[it+1] += num_agents_-1;
    debug_agent_(addr_ser, "NEED ", agent_msgs_needed_[it+1]);

    // Increase the iteration counter
    ++it;
    while (it >= vn_count_-1) {
        vn_wait_.push_back({});
        vn_remaining_.push_back(0);
        ++vn_count_;
    }

    if (vote_stop) {
        num_dormant_ = 0;
        num_inactive_ = graph_.size();
        info_agent_(addr_ser, "VOTE STP|");
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
