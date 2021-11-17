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

#ifdef CONFIG_FULL

void Agent::process_vertices() {
    uint64_t my_agent_ser = addr_ser;

    std::vector<std::pair<vertex_t, it_t> > full_active;

    // Process all active vertices
    while (active_.size() > 0) {
        absl::flat_hash_map<uint64_t, std::vector<VertexNotification> > out_vn_msgs;
        absl::flat_hash_set<vertex_t> new_active;

        it_t max_it = 0;

        // Each active vertex has already been determined to have all
        // running conditions met; we just run it
        for (vertex_t v : active_) {
            // Prepare its notification space
            std::vector<ReplicaLocalStorage> replica_notifications {};
            VertexNotification vertex_notification {};
            bool notify_vertex = false;

            auto & gv = graph_[v];

            if (gv.local.state == DORMANT) {
                dormant_.erase(v);
                --num_dormant_;
                gv.local.state = ACTIVE;
            } else if (gv.local.state == INACTIVE) {
                --num_inactive_;
                gv.local.state = ACTIVE;
            }

            // Run it
            alg_.run(gv, global_nV_, vn_, vn_wait_, vn_remaining_,
                    replica_notifications,
                    vertex_notification, notify_vertex);
            debug_agent_(addr_ser, "RAN VTX | ", v, " it=", gv.local.iteration, " state=", gv.local.state);

            // Make sure we stay one ahead for notifications
            if (gv.local.iteration > max_it)
                max_it = gv.local.iteration;

            // Inspect its state
            if (notify_vertex) {
                vertex_notification.v = v;

                // Send out a vertex update to each neighbor
                for (const auto &n : gv.out_neighbors) {
                    bool dummy;
                    edge_t e;
                    e.src = v;
                    e.dst = n;
                    uint64_t agent_dst = find_agent(e, IN, true, 0, dummy);
                    if (agent_dst == my_agent_ser) {
                        // We can directly add this
                        // Later, we will go through everyone that was
                        // processed and will set active any vertex that
                        // needs it
                        #ifdef CONFIG_PAGERANK
                        vn_[gv.local.iteration][v] = vertex_notification;
                        #endif
                        #ifdef CONFIG_WCC
                        vn_[v] = vertex_notification;
                        #endif
                    } else {
                        out_vn_msgs[agent_dst].push_back(vertex_notification);

                        #ifdef CONFIG_SEND_MSG_EARLY
                        if (out_vn_msgs[agent_dst].size() > SEND_MSG_EARLY_LIMIT) {
                            auto & vn_msgs = out_vn_msgs[agent_dst];
                            size_t msg_size = sizeof(msg_type_t)+sizeof(VertexNotification)*vn_msgs.size();
                            char *msg = new char[msg_size];

                            char *msg_ptr = msg;
                            pack_msg(msg_ptr, OUT_VN);
                            for (const VertexNotification &vn : vn_msgs)
                                pack_single(msg_ptr, vn);

                            ZMQRequester &req = get_requester(agent_dst);
                            req.send(msg, msg_size);

                            delete [] msg;
                            out_vn_msgs[agent_dst].clear();
                        }
                        #endif
                    }
                }
            }

            // At this point, this vertex/it pair is complete
            full_active.push_back({v, gv.local.iteration});

            if (gv.local.state == INACTIVE) {
                ++num_inactive_;
                continue;
            } else if (gv.local.state == DORMANT) {
                dormant_.insert(v);
                ++num_dormant_;
                continue;
            }

            if (gv.local.vertex_recv_needed == 0 &&
                    gv.local.neighbor_recv_needed == 0 &&
                    gv.local.replica_recv_needed == 0) {
                // This vertex can become active again, everything matches
                new_active.insert(v);
            }
        }

        while (max_it >= vn_count_-1) {
            #ifdef CONFIG_PAGERANK
            vn_.push_back({});
            #endif
            vn_wait_.push_back({});
            vn_remaining_.push_back(0);
            ++vn_count_;
        }

        // Now, revisit all vertices and check if they should remain
        // active due to any locally resolved updates or if they cannot
        // Note: everything in full active has passed the given iteration
        for (const auto& [v, it] : full_active) {
            // Check who is waiting on this, and update them appropriately
            bool exists_valid = false;
            for (auto & [waiter, valid] : vn_wait_[it][v]) {
                if (!valid) continue;
                exists_valid = true;
                // Since this value is available, decrease their count,
                // remove it from the list, and set active if appropriate
                valid = false;
                if (--graph_[waiter].local.vertex_recv_needed == 0) {
                    if (graph_[waiter].local.state == ACTIVE) {
                        // If it is now zero, then set active if appropriate
                        if (graph_[waiter].local.neighbor_recv_needed == 0 &&
                                graph_[waiter].local.replica_recv_needed == 0) {
                            new_active.insert(waiter);
                        }
                    }
                }
            }
            if (!exists_valid)
                vn_wait_[it].erase(v);
        }
        active_.swap(new_active);

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
        for (const auto& [agent_dst, vn_msgs] : out_vn_msgs) {
            // We want to send the list of vn msgs to the given agent
            size_t msg_size = sizeof(msg_type_t)+sizeof(VertexNotification)*vn_msgs.size();
            char *msg = new char[msg_size];

            char *msg_ptr = msg;
            pack_msg(msg_ptr, OUT_VN);
            for (const VertexNotification &vn : vn_msgs)
                pack_single(msg_ptr, vn);

            ZMQRequester &req = get_requester(agent_dst);
            req.send(msg, msg_size);

            delete [] msg;
        }
    }

    // Now, check if everyone is inactive/dormant, meaning we need to
    // change states
    if (num_inactive_ + num_dormant_ == nV_) {
        state_ = JOIN_BARRIER;
        pre_poll();
    }
    #ifdef DEBUG_EXCESSIVE
    if (num_dormant_ == nV_-1) {
        for (auto x : active_) {
            info_agent_(addr_ser, " x ", x);
        }
    }
    #endif
}

#endif
