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

#include "absl/container/flat_hash_map.h"

using namespace elga;

std::mutex elga::p_mutex {};

namespace elga::agent {

    void print_usage() {
        std::cout << "Usage: agent [help] ip-address" << std::endl;
    }

    int print_help() {
        std::cout << "\n"
            "Interface to an ElGA agent.\n"
            "The agent is responsible for part of the graph and holds\n"
            "it in memory while managing the algorithm execution on it.\n"
            "Options:\n"
            "    help : display this help message"
            "    ip-address : (required) the IP address to listen on\n" <<
            std::endl;
        return 0;
    }

    int main(int argc, const char **argv, const ZMQAddress &directory_master, localnum_t ln) {
        if (argc <= 1) { print_usage(); return 0; }
        // Do an initial pass over all arguments checking for 'help'
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "help") {
                print_usage();
                return print_help();
            }
        }

        // Form our address
        ZMQAddress addr(argv[1], ln);

        // Create the agent
        elga::Agent agent(addr, directory_master);

        // Register ourselves
        agent.register_dir();

        // Begin the agent
        agent.start();

        return 0;
    }

}

void Agent::register_dir() {
    // Listen for alg messages
    sub(DO_START);
    sub(DO_UPDATE);
    sub(DO_SAVE);
    sub(DO_DUMP);
    sub(DO_RESET);
    sub(DO_CHK_T);
    sub(DO_VA);
    sub(NV);
    sub(RV);
    sub(HAVE_UPDATE);
    sub(SYNC);
    #ifdef CONFIG_CS
    sub(DO_CS_LB);
    #endif
    sub(SIMPLE_SYNC_DONE);
    #ifdef CONFIG_AUTOSCALE
    sub(AS_SCALE);
    #endif

    // Register our number of virtual agents
    char data[pack_msg_agent_size];
    char *data_ptr = data;
    pack_msg_agent(data_ptr, AGENT_JOIN, addr_ser, vagent_count_);
    d_req_.send(data, sizeof(data));

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Agent] registered with directory" << std::endl;
    #endif
}

uint64_t Agent::get_owner(update_t& u) {
    bool dummy;
    return find_agent(u.e, u.et, true, 0, dummy);
}

void Agent::change_edge(update_t u, bool count_deg) {
    // Ensure this edge is destined for us; if not, prep it for later moves
    uint64_t owner = get_owner(u);
    if (owner != addr_ser) {
        debug_agent_(addr_ser, "got ", u.e.src, "->", u.e.dst, " me=", addr_ser, " owner=", owner);
        moves[owner].push_back(u);
        return;
    }

    vertex_t v_mine = (u.et == IN) ? u.e.dst : u.e.src;
    vertex_t v_theirs = (u.et == IN) ? u.e.src : u.e.dst;

    #ifdef CONFIG_WCC
    tmap[v_theirs].push_back(v_mine);
    #endif

    VertexStorage &vs = graph_[v_mine];
    if (vs.vertex != v_mine)
        vs.vertex = v_mine;
    auto &neighbors = (u.et == IN) ? vs.in_neighbors : vs.out_neighbors;

    // Set the vertex to active
    if (vs.local.state != DORMANT) {
        vs.local.state = ACTIVE;
        #if !defined(CONFIG_BSP) && !defined(CONFIG_LBSP)
        active_.insert(v_mine);
        #endif
    }

    // Add this edge to the graph
    if (u.insert) {
        if (vs.in_neighbors.size() == 0 && vs.out_neighbors.size() == 0) {
            nV_++;
            update_nV_set_.insert(v_mine);
        }
        neighbors.push_back(v_theirs);
        if (u.et == IN) {
            update_nE_++;
            nE_++;
        }
        #ifdef CONFIG_CS
        // Update the sketch
        if (count_deg) {
            int32_t deg_est = cms.query_count(v_mine);
            if (deg_est >= REP_THRESH) {
                // We want to push our sketch up
                push_sketch_ = true;
            }
        }
        #endif
    } else {
        if (vs.out_neighbors.size() == 0 && vs.in_neighbors.size() == 0) {
            nV_--;
            update_nV_ -= 1.0/vs.replicas.size();
            graph_.erase(v_mine);
        }
        if (neighbors.size() == 1)
            neighbors.clear();
        else {
            std::iter_swap(std::find(neighbors.begin(), neighbors.end(), v_theirs),
                    neighbors.end()-1);
            neighbors.pop_back();
        }

        if (u.et == IN) {
            update_nE_--;
            nE_--;
        }
    }
}

void Agent::pre_poll() {
    #ifdef CONFIG_AUTOSCALE
    if (dead) return;
    #endif
    // This implements a BSP-like processing:
    // 1) for every active vertex, or those with active neighbors, process
    // in the algorithm
    // 2) send updates to each out-agent, even if the updates are empty
    // 3) wait for in-agent number of updates
    // 4) join the global barrier
    if (state_ == PROCESS) {
        // Process each vertex, and send out all out-agent updates
        process_vertices();
    } else if (state_ == JOIN_BARRIER) {
        // We are at the barrier, so we need to signal as such
        char msg[sizeof(msg_type_t)+sizeof(size_t)];
        char *msg_ptr = msg;
        pack_msg(msg_ptr, READY_SYNC);
        pack_single(msg_ptr, (size_t)(num_dormant_));

        d_req_.send(msg, sizeof(msg));

        state_ = WAIT_FOR_SYNC;

        debug_agent_(addr_ser, "WAIT-S  |");
    } else if (state_ == WAIT_FOR_SYNC) {
        // Do nothing, a sync will be explicitly handled with a different
        // message or later
    }
}

bool Agent::shutdown() {
    if (state_ == NO_PROCESS) state_ = IDLE;
    info_agent_(addr_ser, "LEAVE   |");

    if (nE_ == 0 && update_acks_needed_ == 0) {
        info_agent_(addr_ser, "SHUTDOWN|");
        return false;
    }

    char data[pack_msg_agent_size];
    char *data_ptr = data;
    pack_msg_agent(data_ptr, AGENT_LEAVE, addr_ser, vagent_count_);
    d_req_.send(data, sizeof(data));

    return true;
}

#ifdef CONFIG_CS
void Agent::push_sketch() {
    // We want to send our local sketch up to the directory, then reset
    info_agent_(addr_ser, "SEND SK |");

    size_t cms_size = CountMinSketch::size();
    size_t msg_size = sizeof(msg_type_t);
    if (push_sketch_) msg_size += cms_size;

    char *msg = new char[msg_size];
    char *data = msg;

    pack_msg(data, CS_UPDATE);

    if (push_sketch_) {
        const char* cms_ser = cms.serialize();
        memcpy(data, cms_ser, cms_size);
    }

    d_req_.send(msg, msg_size);

    delete [] msg;

    if (push_sketch_) cms.clear();

    push_sketch_ = false;
    state_ = WAIT_FOR_LB;
}

#endif

void Agent::handle_directory_update() {
    if (state_ == WAIT_FOR_LB_SYNC) {
        info_agent_(addr_ser, "DIR UPD | [----]");
        return;
    }
    if (state_ == WAIT_FOR_LB) {
        // Hold off on an update
        info_agent_(addr_ser, "DIR UPD | [wait]");
        state_ = WAIT_FOR_LB_SYNC;

        // Instead, signal that we received it
        char msg[sizeof(msg_type_t)];
        char *msg_ptr = msg;
        pack_msg(msg_ptr, SIMPLE_SYNC);

        d_req_.send(msg, sizeof(msg));

        return;
    }
    if (state_ == NO_PROCESS && false) {
        info_agent_(addr_ser, "DIR UPD | [skip]");
        return;
    }
    // The directory was updated
    // We want to see if we lost any edges and, if so, move them
    timer::Timer du_t {"directory-update"};
    size_t lost_edges = 0;
    size_t lost_out_edges = 0;
    absl::flat_hash_set<vertex_t> v_to_remove;
    du_t.tick();
    for (auto & [v_o, lv_o] : graph_) {
        auto &v = v_o;
        auto &lv = lv_o;
        lv.out_neighbors.erase(std::remove_if(lv.out_neighbors.begin(),
                lv.out_neighbors.end(), [&](vertex_t n) {
                    bool dummy;
                    edge_t e;
                    e.src = v;
                    e.dst = n;
                    uint64_t cur_agent = find_agent(e, OUT, true, 0, dummy);
                    if (cur_agent != addr_ser) {
                        debug_agent_(addr_ser, "MOVE EDG | ", v, "->", n);
                        update_t u;
                        u.e = e;
                        u.et = OUT;
                        u.insert = true;
                        moves[cur_agent].push_back(u);
                        ++lost_out_edges;
                        return true;
                    }
                    return false;
                }), lv.out_neighbors.end());
        lv.in_neighbors.erase(std::remove_if(lv.in_neighbors.begin(),
                lv.in_neighbors.end(), [&](vertex_t n) {
                    bool dummy;
                    edge_t e;
                    e.src = n;
                    e.dst = v;
                    uint64_t cur_agent = find_agent(e, IN, true, 0, dummy);
                    if (cur_agent != addr_ser) {
                        debug_agent_(addr_ser, "MOVE EDG | ", v, "<-", n);
                        update_t u;
                        u.e = e;
                        u.et = IN;
                        u.insert = true;
                        moves[cur_agent].push_back(u);
                        ++lost_edges;
                        return true;
                    }
                    return false;
                }), lv.in_neighbors.end());
        if (lv.out_neighbors.size() == 0 && lv.in_neighbors.size() == 0)
            v_to_remove.insert(v);
    }
    nE_ -= lost_edges;
    info_agent_(addr_ser, "EDGE RM | ", lost_out_edges, " + ", lost_edges);

    // Remove the vertices as appropriate
    for (vertex_t v : v_to_remove)
        graph_.erase(v);
    nV_ -= v_to_remove.size();

    // Now, move the actual edges
    send_move_edges();

    if (update_acks_needed_ != 0) {
        move_timer_.tick();
        state_ = WAIT_EDGE_MOVE;
    }

    du_t.tock();
    info_agent_(addr_ser, "DIR UPD | ", du_t);
}

void Agent::send_move_edges() {
    if (moves.size() == 0) return;

    size_t moved_edges = 0;
    for (auto & [agent, moved_changes]: moves) {
        const uint8_t flag_move_edges = 0x0;
        size_t msg_size = sizeof(msg_type_t)+sizeof(flag_move_edges)+sizeof(addr_ser)+moved_changes.size()*sizeof(update_t);
        char *msg = new char[msg_size];

        char *msg_ptr = msg;

        pack_msg(msg_ptr, SEND_UPDATES);
        pack_single(msg_ptr, flag_move_edges);
        pack_single(msg_ptr, addr_ser);
        for (const update_t & u : moved_changes) {
            pack_single(msg_ptr, u);
            ++moved_edges;
        }

        ZMQRequester &req = get_requester(agent);
        req.send(msg, msg_size);

        delete [] msg;

        moved_changes.clear();
    }
    update_acks_needed_ += moves.size();

    info_agent_(addr_ser, "MOVED   | ", moved_edges, " to ", moves.size());

    moves.clear();
}

void Agent::process_vn(const char *data, size_t size) {
    // Ignore any trailing end-of-batch messages
    if (state_ == IDLE) return;

    const char *end = data+size;
    #if defined(CONFIG_BSP) || defined(CONFIG_LBSP)
    it_t it;
    unpack_single(data, it);
    while (it >= vn_count_) {
        vn_wait_.push_back({});
        #ifdef CONFIG_BSP
        vn_.push_back({});
        #endif
        vn_remaining_.push_back(0);
        ++vn_count_;
    }

    #endif
    while (data < end) {
        VertexNotification vn;
        unpack_single(data, vn);

        vertex_t v = vn.v;

        #ifdef CONFIG_NOTIFY_AGG
        auto n = vn.n;
        {
            bool dummy;
            edge_t e;
            e.src = v;
            e.dst = n;
            uint64_t agent_dst = find_agent(e, IN, true, 0, dummy);
            if (agent_dst != addr_ser) {
                throw std::runtime_error("outside msg received");
            }
        }
        debug_agent_(addr_ser, "RECEIVED VN : from ", v, " to ", n);
        #endif

        #if !defined(CONFIG_BSP) && !defined(CONFIG_LBSP)
        it_t it = vn.it;

        debug_agent_(addr_ser, "RECEIVED VN | ", vn.v, "@", vn.it);

        // NOTE: can add a flag here to make all neighbors active via
        // a vertex notification also

        // Install the vertex notification
        while (it >= vn_count_) {
            vn_wait_.push_back({});
            vn_.push_back({});
            vn_remaining_.push_back(0);
            ++vn_count_;
        }

        #endif
        #ifdef CONFIG_BSP
        vn_[it][v] = vn;
        #endif
        #ifdef CONFIG_LBSP
        for (vertex_t n : tmap[v]) {
            #ifdef CONFIG_TACTIVATE
            // Make sure the vertex is in the graph, in case of stale edges
            if (graph_.count(n) > 0) {
                // Make sure it will actually get changed
                if (graph_[n].local.new_cc > vn.cc) {
                    graph_[n].local.new_cc = vn.cc;
                    // If it's dormant, leave it
                    if (graph_[n].local.state == INACTIVE) {
                        tactivate[it].insert(n);
                    }
                }
            }
            #else
            if (graph_.count(n) > 0) {
                alg_.set_active(graph_[n], vn);
            }
            #endif
        }
        #ifndef CONFIG_TACTIVATE
        vn_[v] = vn;
        #endif
        #endif

        #if !defined(CONFIG_BSP) && !defined(CONFIG_LBSP)
        // Mark any vertices waiting on it to be processed
        bool exists_valid = false;
        auto &waiters = vn_wait_[it][v];
        for (auto & [waiter, valid] : waiters) {
            if (!valid) continue;

            debug_agent_(addr_ser, "VN WAIT | ", " vn.v=", v, " vn.it=", it, " waiter=", waiter, " .vrc=" ,graph_[waiter].local.vertex_recv_needed);

            exists_valid = true;

            // Since this value is available, decrease their count,
            // remove it from the list, and set active if appropriate
            valid = false;
            if (--graph_[waiter].local.vertex_recv_needed == 0) {
                // If it is now zero, then set active if appropriate
                if (graph_[waiter].local.neighbor_recv_needed == 0 &&
                        graph_[waiter].local.replica_recv_needed == 0) {
                    if (graph_[waiter].local.state == ACTIVE) {
                        active_.insert(waiter);
                    }
                }
            }
        }
        if (!exists_valid)
            vn_wait_[it].erase(v);
        #endif
    }
    #if defined(CONFIG_BSP) || defined(CONFIG_LBSP)
    --agent_msgs_needed_[it];
    if (state_ == PROCESS && it_ >= 0 && agent_msgs_needed_[it_+1] == 0)
        state_ = JOIN_BARRIER;
    #endif

    // Now, actually update the graph based on this input
    pre_poll();
}

bool Agent::handle_msg(zmq_socket_t sock, msg_type_t t, const char *data, size_t size) {
    #ifdef CONFIG_AUTOSCALE
    if (dead && t != AS_SCALE && t != QUERY) return true;
    #endif
    switch (t) {
        #ifdef CONFIG_AUTOSCALE
        case AS_SCALE: {
            const char *end = data+size;
            scale_direction dir;
            unpack_single(data, dir);

            bool us = false;
            while (data < end) {
                uint64_t target;
                unpack_single(data, target);
                uint64_t agent_target;
                aid_t vagents_target;

                unpack_agent(target, agent_target, vagents_target);
                if (agent_target == addr_ser) {
                    us = true;
                    break;
                }
            }

            if (!us) break;

            // This is intended for us
            info_agent_(addr_ser, "SCALE   | received scale request in direction ", dir);

            if (dir == SCALE_IN) {
                dying = true;
                state_ = IDLE;
                char data[pack_msg_agent_size];
                char *data_ptr = data;
                pack_msg_agent(data_ptr, AGENT_LEAVE, addr_ser, vagent_count_);
                d_req_.send(data, sizeof(data));

                break;
            } else if (dir == SCALE_OUT) {
                register_dir();
                dead = false;
                dying = false;
            }

            break;
        }
        #endif
        case QUERY: {
            vertex_t v;
            unpack_single(data, v);

            size_t resp_size;
            resp_size = alg_.query_resp_size();

            ZMQMessage resp { sock, resp_size };
            char* resp_data = resp.edit_data();

            if (graph_.count(v) > 0)
                alg_.query(resp_data, graph_[v]);
            else
                alg_.query(resp_data);

            resp.send();

            #ifdef CONFIG_AUTOSCALE
            ++query_count_;
            #endif

            break;
        }
        case OUT_VN: {
                         debug_agent_(addr_ser, "OUT VN  |");
                         // We received vertex notifications, we need to
                         // handle them
                         process_vn(data, size);
                         break;
                     }
        case SEND_UPDATES: {
                               // We are receiving a batch of updates to
                               // perform in bulk, to match in with out
                               // edges
                               // Iterate through and process all updates
                               uint8_t count_deg = *(const uint8_t*)data;
                               data += sizeof(uint8_t);
                               uint64_t resp_aser = *(const uint64_t*)data;
                               data += sizeof(uint64_t);
                               size_t num_updates = size/sizeof(update_t);
                               for (size_t ctr = 0; ctr < num_updates; ++ctr) {
                                   update_t u = ((const update_t*)data)[ctr];
                                   if (count_deg == 0x2) {
                                       // This is a check
                                       // We received OUT edges
                                       // Ensure that we have the edge
                                       if (graph_.count(u.e.src) == 0) throw std::runtime_error("R Check failed; vertex not found");
                                       bool found = false;
                                       for (auto &n : graph_[u.e.src].out_neighbors) {
                                           if (n == u.e.dst) {
                                               found = true;
                                               break;
                                           }
                                       }
                                       if (!found) throw std::runtime_error("R Check failed; edge not found");
                                   } else {
                                       change_edge(u, count_deg == 0x1);
                                   }
                               }
                               if (count_deg == 0x2) break;     // do nothing more if just a check
                               // Offload any new moves
                               send_move_edges();

                               // Send back the acknowledgement
                               char msg[sizeof(msg_type_t)];
                               *(msg_type_t*)msg = ACK_UPDATES;
                               ZMQRequester &req = get_requester(resp_aser);
                               req.send(msg, sizeof(msg));
                               break;
                           }
        case DO_CHK_T: {
                            // Send all out-edges as an 'update', to check the
                            // transpose
                            debug_agent_(addr_ser, "starting transpose check");
                            send_out_edges(true);
                            break;
                        }
        case DO_VA: {
                        balance_va();
                        break;
                    }
        case ACK_UPDATES: {
                                // Check if we are at the right count for
                                // updates
                                debug_agent_(addr_ser, "GOTACK  | ", update_acks_needed_);
                                if (--update_acks_needed_ != 0) break;

                                debug_agent_(addr_ser, "SENDRNV | ", update_acks_needed_);

                                if (state_ == WAIT_EDGE_MOVE) {
                                    move_timer_.tock();
                                    info_agent_(addr_ser, "MOVED T | ", move_timer_, " =", time(NULL), "=");
                                    move_timer_.reset();
                                    state_ = IDLE;
                                    break;
                                }

                                done_waiting_ready_nv_ne();

                                break;
                          }
        case UPDATE_EDGES: {
                               // We are receiving edges to update in bulk
                               // Set them all appropriately
                               size_t num_updates = size/sizeof(update_t);
                               for (size_t ctr = 0; ctr < num_updates; ++ctr) {
                                   update_t u = ((const update_t*)data)[ctr];
                                   if (state_ == NO_PROCESS)
                                       change_edge(u);
                                   else update_set_.insert(u);
                               }
                               break;
                           }
        case UPDATE_EDGE: {
                              // Extract the update
                              update_t u;
                              unpack_update(data, u);
                              // There are two cases:
                              //    1) we are in a non-processing stage,
                              //    and we should insert/delete
                              //    immediately
                              if (state_ == NO_PROCESS) {
                                  change_edge(u);

                              //    2) we are following a batch cycle, and
                              //    we need to
                              //        A) add this to our graph change
                              //        set
                              //        B) if we are in an idle state, we
                              //        need to indicate that we have
                              //        a new message
                              //        (send a HAVE_UPDATE message)
                              } else {
                                  debug_agent_(addr_ser, "UP EDGE | ", state_);
                                  update_set_.insert(u);
                                  if (state_ == IDLE) {
                                      start_leaving_idle();
                                  }
                              }
                              break;
                          }
        case HAVE_UPDATE: {
                              // At least some agent has updates
                              // Now, process our update set, then wait for
                              // out edges as necessary before beginning
                              state_ = FINALIZE_GRAPH_BATCH;

                              update_timer_.reset();
                              update_timer_.tick();

                              // Make sure this is the right batch
                              batch_t have_update_batch;
                              unpack_batch(data, have_update_batch);
                              if (have_update_batch != batch_) throw std::runtime_error("Received wrong batch have update from directory");

                              finalize_graph_batch();
                              break;
                          }
        case DO_UPDATE: {
                        if (state_ != NO_PROCESS) throw std::runtime_error("Update only valid before processing");
                        update_timer_.reset();
                        update_timer_.tick();
                        send_out_edges();
                        break;
                     }
        #ifdef CONFIG_CS
        case DO_CS_LB: {
                        push_sketch();
                        break;
                    }
        #endif
        case DO_START: {
                        #ifdef CONFIG_START_VTX
                        vertex_t start;
                        unpack_single(data, start);
                        alg_.set_start(start);
                        info_agent_(addr_ser, "START V | ", start);
                        #endif
                        // Move out of the pre-processing state and into
                        // the full state
                        if (state_ == NO_PROCESS) {
                            debug_agent_(addr_ser, "received START");

                            state_ = LEAVING_NO_PROCESS;

                            update_timer_.tick();

                            // To leave the NO_PROCESS state, we need to
                            // ensure that all of our IN edges will have
                            // corresponding OUT edges, and then update the
                            // batch statistics, similar to leaving IDLE
                            send_out_edges();
                        } else if (state_ == IDLE) {
                            // In other cases, this means that the user
                            // wants to run again without any changes.
                            // This could be for timing or testing reasons
                            update_timer_.reset();
                            update_timer_.tick();
                            start_leaving_idle();
                        } else throw std::runtime_error("START from unknown state: " + std::to_string(state_));
                        break;
                    }
        case DO_SAVE: {
                       debug_agent_(addr_ser, "saving alg results");
                       save();
                       break;
                   }
        case DO_DUMP: {
                       debug_agent_(addr_ser, "dumping graph to disk");
                       dump();
                       break;
                   }
        case DO_RESET: {
                            clear_batch_mem();
                            for (auto & [v, vs] : graph_)
                                alg_.reset_output(vs);
                            break;
                       }
        case RV:    {
                        uint64_t src_agent;
                        unpack_single(data, src_agent);
                        size -= sizeof(uint64_t);
                        size_t num_updates = size/(sizeof(it_t)+sizeof(vertex_t)+sizeof(ReplicaLocalStorage));
                        for (size_t ctr = 0; ctr < num_updates; ++ctr) {
                            it_t it;
                            unpack_single(data, it);
                            vertex_t v;
                            unpack_single(data, v);
                            ReplicaLocalStorage rep;
                            unpack_single(data, rep);
                            // Insert the replica values
                            if (graph_.count(v) == 0) {
                                debug_agent_(addr_ser, "MAKE NEW=", v);
                                // This is a new vertex; we need to compute the
                                // number of replicas (even though we have
                                // zero, it exists elsewhere)
                                graph_[v].vertex = v;
                                graph_[v].self = addr_ser;
                                auto reps = ch_.find(v);
                                for (auto & rep : reps) {
                                    uint64_t agent_ser;
                                    aid_t aid;
                                    unpack_agent(rep, agent_ser, aid);
                                    graph_[v].replicas.insert(agent_ser);
                                }
                            }
                            graph_[v].replica_storage[it][src_agent] = rep;
                            #ifdef CONFIG_LBSP
                            alg_.set_rep_active(graph_[v], rep);
                            #endif
                        }
                        break;
                    }
        case NV: {
                     // We are ready to enter the processing step
                     // First, extract and set the global nV/nE values
                     global_nV_ = *(size_t*)data;
                     global_nE_ = *(size_t*)(data+sizeof(size_t));

                     // Next, begin computation
                     debug_agent_(addr_ser, "GOT NV  | ", global_nV_, " ", global_nE_);

                     update_timer_.tock();
                     info_agent_(addr_ser, "UPDATE  | ", update_timer_);

                     batch_timer_.tick();

                     // Setup the iteration state variables
                     #ifdef CONFIG_BSP
                     vn_.push_back({});
                     vn_.push_back({});
                     #endif
                     vn_wait_.push_back({});
                     vn_wait_.push_back({});
                     vn_count_ = 2;
                     vn_remaining_.push_back(0);
                     vn_remaining_.push_back(0);

                     ss_timer_.tick();

                     if (state_ == NO_PROCESS) {
                        state_ = JOIN_BARRIER;
                        pre_poll();
                        break;
                     }

                     state_ = PROCESS;
                     break;
                 }
        case SIMPLE_SYNC_DONE: {
                                    if (state_ == WAIT_FOR_LB_SYNC) {
                                        // Perform the actual directory update
                                        state_ = IDLE;
                                        handle_directory_update();
                                        break;
                                    } else
                                        throw std::runtime_error("Simple sync from unknown state");
                               }
        case SYNC: {
                       if (state_ != WAIT_FOR_SYNC) { info_agent_(addr_ser, "Unknown control flow: ", state_); throw std::runtime_error("Unknown control flow"); }
                       size_t global_num_active = *(size_t*)data;
                       debug_agent_(addr_ser, "SYNC    | ", global_num_active);
                       if (global_num_active == 0) {
                           ss_timer_.tock();
                           info_agent_(addr_ser, "SUP STP | ", ss_timer_);

                           // Clear out the batch memory being used
                           clear_batch_mem();

                           state_ = IDLE;
                           batch_timer_.tock();
                           // Increase our batch number
                           ++batch_;
                           info_agent_(addr_ser, "B TIME  | ", batch_timer_);

                           // Now, check if we have any updates queued up
                           // If so, we need to begin the next batch now
                           if (update_set_.size() > 0)
                               start_leaving_idle();
                       } else {
                           // It is not a new batch, we need to continue
                           // computing
                           gc();
                           // Update our state
                           ss_timer_.tock();
                           info_agent_(addr_ser, "SUP STP | ", ss_timer_);
                           #ifdef CONFIG_TIME_FIND_AGENTS
                           info_agent_(addr_ser, "F TIME  | ", find_agent_t);
                           find_agent_t.reset();
                           #endif
                           ss_timer_.tick();
                           state_ = PROCESS;
                           // Move everyone that was dormant to be procesesed
                           move_dormant_active();
                           // Perform an update/process
                           pre_poll();
                       }
                       break;
                   }
        default: { return false; }
    }
    #ifdef CONFIG_AUTOSCALE
    if (dead == false && dying == true && nE_ == 0 && update_acks_needed_ == 0)
        dead = true;
    #endif
    return true;
}

void Agent::send_out_edges(bool check) {
    // Note, we cannot go back into a NO_PROCESS state, so this is
    // a one-time operation
    // We essentially copy and transpose the entire graph and send
    // it to the appropriate neighbors

    timer::Timer soe_t {"send_out_edges"};
    soe_t.tick();

    if (!check) {
        // Remove multi-edges
        for (auto &ve : graph_) {
            auto &in = ve.second.in_neighbors;

            std::sort(in.begin(), in.end());
            auto last_in = std::unique(in.begin(), in.end());
            nE_ -= in.end() - last_in;
            in.erase(last_in, in.end());
        }
    }

    absl::flat_hash_map<uint64_t, std::vector<update_t> > updates_to_send;

    uint64_t my_agent_ser = addr_ser;
    bool dummy;

    std::vector<update_t> my_insertions;

    for (auto &ve : graph_) {
        for (auto &n : ve.second.in_neighbors) {
            // Register the appropriate edge to send out to
            edge_t e;
            e.src = n;
            e.dst = ve.second.vertex;
            uint64_t agent_dst = find_agent(e, OUT, true, 0, dummy);
            update_t u;
            u.e = e;
            u.et = OUT;
            u.insert = true;

            if (agent_dst == my_agent_ser) {
                // This is easy, just insert the edge
                my_insertions.push_back(u);
            } else
                updates_to_send[agent_dst].push_back(u);
        }
    }
    debug_agent_(addr_ser, "SEND UPDATES", updates_to_send.size());

    // Now, send each block of edges
    size_t max_size = 0;
    for (const auto& [agent_ser, updates] : updates_to_send) {
        uint8_t flag_out_edges = 0x1;
        if (check) flag_out_edges = 0x2;
        size_t msg_size = sizeof(msg_type_t)+sizeof(flag_out_edges)+sizeof(addr_ser)+sizeof(update_t)*updates.size();
        ZMQRequester &req = get_requester(agent_ser);
        #ifdef CONFIG_PREPARE_SEND
        auto msg = req.prepare_send(msg_size);
        char *msg_ptr = msg.edit_data();
        #else
        char *msg = new char[msg_size];
        char *msg_ptr = msg;
        #endif
        pack_msg(msg_ptr, SEND_UPDATES);
        pack_single(msg_ptr, flag_out_edges);
        pack_single(msg_ptr, addr_ser);

        for (const update_t &u : updates) {
            pack_single(msg_ptr, u);
        }

        #ifdef CONFIG_PREPARE_SEND
        msg.send();
        #else
        req.send(msg, msg_size);
        delete [] msg;
        #endif
    }


    // Next, process our insertions
    for (auto &u : my_insertions) {
        if (!check) change_edge(u);
        else {
            // Ensure that we have the edge
            if (graph_.count(u.e.src) == 0) throw std::runtime_error("Check failed; vertex not found");
            bool found = false;
            for (auto &n : graph_[u.e.src].out_neighbors) {
                if (n == u.e.dst) {
                    found = true;
                    break;
                }
            }
            if (!found) throw std::runtime_error("Check failed; edge not found");
        }
    }

    soe_t.tock();

    // Save the confirmations required, and if there are none left then
    // progress to the ready state
    if (!check) {
        update_acks_needed_ += updates_to_send.size();
        if (update_acks_needed_ == 0)
            done_waiting_ready_nv_ne();
    } else
        info_agent_(addr_ser, "SND CHK | PASSED");

    debug_agent_(addr_ser, "SENDOUT | ", soe_t, " want acks:", update_acks_needed_);
}

#ifdef CONFIG_AUTOSCALE
void Agent::track_query_rate() {
    query_rate_t_.tock();
    query_rate_ = query_count_/query_rate_t_.get_time().count();
    query_count_ = 0;
    query_rate_t_.tick();

    #ifdef SEND_ONLY_THRESHOLD
    if (query_rate_ > AUTOSCALE_QUERY_RATE_THRESHOLD_HIGH || query_rate_ < AUTOSCALE_QUERY_RATE_THRESHOLD_LOW) {
        info_agent_(addr_ser, "THRESH  |");
    #endif
        // Report up
        size_t msg_size = sizeof(msg_type_t)+sizeof(uint64_t)+sizeof(double);
        char msg[msg_size];
        char *msg_ptr = msg;

        pack_msg_agent(msg_ptr, AS_QUERY, addr_ser, vagent_count_);
        pack_single(msg_ptr, query_rate_);

        d_req_.send(msg, msg_size);
    #ifdef SEND_ONLY_THRESHOLD
    }
    #endif
}
#endif

bool Agent::heartbeat() {
    if (ZMQChatterbox::heartbeat() == false)
        return false;

    #ifdef CONFIG_AUTOSCALE
    if (dead) {
        info_agent_(addr_ser, "DEAD    |");
        return true;
    }
    #endif

    #ifdef CONFIG_TIME_INGESTION
    ingest_t_.tock();
    size_t new_edges = nE_-last_edges_;
    last_edges_ = nE_;
    double rate = 0.;
    if (new_edges > 0)
        rate = new_edges/ingest_t_.get_time().count();
    ingest_t_.tick();
    #endif

    #ifdef CONFIG_AUTOSCALE
    track_query_rate();
    #endif

    // Print out our current number of vertices and edges
    info_agent_(addr_ser,
            "HRTBEAT | ",
            (working_)?"W":"-",
            #ifdef CONFIG_TIME_INGESTION
            " rate=", rate,
            #endif
            #ifdef CONFIG_AUTOSCALE
            " qrate=", query_rate_,
            #endif
            " state=", state_,
            " batch=", batch_,
            " nV=", nV_,
            " nE=", nE_,
            " gnV=", global_nV_,
            " gnE=", global_nE_,
            " pending=", update_set_.size(),
            " ia=", num_inactive_,
            " d=", num_dormant_,
            #if defined(CONFIG_BSP) || defined(CONFIG_LBSP)
            " it=", it_,
            " amn=", agent_msgs_needed_[it_+1],
            #endif
            " uan=", update_acks_needed_
            );

    return true;
}

void Agent::save() {
    // Save the current results in a text format to disk
    timer::Timer t("save_timer");
    t.tick();
    std::stringstream ofn;

    ofn << SAVE_DIR << '/' << addr_ser << ".out";

    if (std::ofstream of {ofn.str()}) {
        for (auto & [v, ve] : graph_)
            alg_.save(of, ve);
    } else
        throw std::runtime_error("Error opening output file");

    t.tock();
    info_agent_(addr_ser, "SAVE T  | ", t);
}

void Agent::dump() {
    // Save our portion of the graph to disk
    #ifdef DUMP_EL
    std::stringstream ofn;

    ofn << SAVE_DIR << '/' << addr_ser << ".el.dump";

    if (std::ofstream of {ofn.str()}) {
        for (auto & [v, ve] : graph_)
            for (auto & n : ve.out_neighbors)
                of << v << " " << n << "\n";
    } else
        throw std::runtime_error("Unable to dump graph");
    #endif

    #ifdef DUMP_BL
    std::stringstream ofd;

    ofd << SAVE_DIR << '/' << addr_ser << ".bl.dump";

    if (std::ofstream of {ofd.str()}) {
        for (auto & [v, ve] : graph_) {
            of << v << '\t' << ve.out_neighbors.size()
                ;
            for (auto & n : ve.out_neighbors)
                of << ' ' << n;
            of << '\n';
        }
    } else
        throw std::runtime_error("Unable to dump blogel graph");
    #endif

    #ifdef DUMP_BL_SYM
    std::stringstream ofsd;

    ofsd << SAVE_DIR << '/' << addr_ser << ".sbl.dump";

    if (std::ofstream of {ofsd.str()}) {
        for (auto & [v, ve] : graph_) {
            of << v << '\t' << ve.out_neighbors.size()
                + ve.in_neighbors.size()
                ;
            for (auto & n : ve.out_neighbors)
                of << ' ' << n;
            for (auto & n : ve.in_neighbors)
                of << ' ' << n;
            of << '\n';
        }
    } else
        throw std::runtime_error("Unable to dump blogel graph");
    #endif

    #ifdef DUMP_OVN_STATE
    std::stringstream ovn;

    ovn << SAVE_DIR << '/' << addr_ser << ".ovn";

    if (std::ofstream of {ovn.str()}) {
        size_t it = 0;
        for (auto & e : vn_) {
            of << it++;
            for (auto & [v, ve] : e)
                alg_.dump_ovn_state(of, v, ve);
            of << "\n";
        }
    } else
        throw std::runtime_error("Unable to dump state");
    #endif

    #ifdef CONFIG_CS
    #ifdef DUMP_CS_STATE
    std::stringstream ofcs;

    ofcs << SAVE_DIR << '/' << addr_ser << ".cs.dump";

    if (std::ofstream of {ofcs.str()}) {
        for (auto & [v, ve] : graph_) {
            of << v;
            of << " " << ve.in_neighbors.size();
            of << " " << ve.out_neighbors.size();
            of << " " << cms.query(v);
            of << " " << rm_.query(v);
            of << " " << rm_.sk_query(v);
            of << " " << count_agent_reps(v);
            of << " " << ve.replicas.size();
            of << '\n';
        }
    } else
        throw std::runtime_error("Unable to dump counts");
    #endif
    #endif
}

void Agent::start_leaving_idle() {
    // If we have already started leaving idle, do not continue spamming
    // the directory
    if (requested_leave_idle_) return;

    // Send a message to the directory
    char msg[pack_msg_batch_size];
    char *msg_ptr = msg;
    pack_msg_batch(msg_ptr, HAVE_UPDATE, batch_);
    d_req_.send(msg, sizeof(msg));

    requested_leave_idle_ = true;
}

void Agent::done_waiting_ready_nv_ne() {
    // We need to signal that we are done waiting
    // and ready to share our nV and nE update
    // values, because those only depend on IN
    // and we will not receive any more
    char msg[pack_msg_unv_une_size];
    char *msg_ptr = msg;
    #ifdef CONFIG_CS
    for (auto & [v_o, lv_o] : graph_) {
        auto &v = v_o;
        auto &lv = lv_o;
        lv.replicas.clear();
        if (count_agent_reps(v) > 0) {
            auto reps = ch_.find(v);
            for (auto & rep : reps) {
                uint64_t agent_ser;
                aid_t aid;
                unpack_agent(rep, agent_ser, aid);
                lv.replicas.insert(agent_ser);
            }
        }
        if (lv.replicas.size() == 0)
            // This can happen when all replicas are virtual agents
            lv.replicas.clear();
    }
    #endif
    for (vertex_t v : update_nV_set_) {
        if (graph_.count(v) > 0) {
            if (graph_[v].replicas.size() > 0)
                update_nV_ += 1./graph_[v].replicas.size();
            else
                update_nV_ += 1.;
        }
    }
    pack_msg_unv_une(msg_ptr, READY_NV_NE, update_nV_, update_nE_);

    d_req_.send(msg, sizeof(msg));

    update_nV_ = 0.;
    update_nV_set_.clear();
    update_nE_ = 0;
}

void Agent::move_dormant_active() {
    // Move dormant vertices to active as appropriate
    #if defined(CONFIG_BSP) || defined(CONFIG_LBSP)
    num_dormant_ = 0;
    for (auto & [v, gv] : graph_) {
        #ifdef CONFIG_LBSP
        if (gv.local.state == DORMANT)
        #endif
        gv.local.state = ACTIVE;
    }
    #else
    for (vertex_t v : dormant_) {
        graph_[v].local.state = ACTIVE;
        if (graph_[v].local.vertex_recv_needed == 0 &&
                graph_[v].local.neighbor_recv_needed == 0 &&
                graph_[v].local.replica_recv_needed == 0) {
            active_.insert(v);
        }
    }
    dormant_.clear();
    num_dormant_ = 0;
    #endif
}

void Agent::finalize_graph_batch() {
    // Move all of the graph updates into the graph, and while doing so
    // create the corresponding out edges
    absl::flat_hash_map<uint64_t, std::vector<update_t> > updates_to_send;

    uint64_t my_agent_ser = addr_ser;
    bool dummy;

    std::vector<update_t> my_insertions;

    for (auto &u : update_set_) {
        // Process this update into our graph
        change_edge(u);

        // Register the appropriate edge to send out to
        uint64_t agent_dst = find_agent(u.e, OUT, true, 0, dummy);
        update_t new_u = u;
        new_u.et = OUT;

        if (agent_dst == my_agent_ser) {
            // Process the OUT ourselves
            change_edge(new_u);
        } else
            updates_to_send[agent_dst].push_back(u);
    }
    update_set_.clear();

    // Now, send each block of edges
    for (auto& [agent_ser, updates] : updates_to_send) {
        const uint8_t flag_out_edges = 0x1;
        size_t msg_size = sizeof(msg_type_t)+sizeof(flag_out_edges)+sizeof(addr_ser)+sizeof(update_t)*updates.size();
        ZMQRequester &req = get_requester(agent_ser);
        #ifdef CONFIG_PREPARE_SEND
        auto msg = req.prepare_send(msg_size);
        char *msg_ptr = msg.edit_data();
        #else
        char *msg = new char[msg_size];
        char *msg_ptr = msg;
        #endif
        pack_msg(msg_ptr, SEND_UPDATES);
        pack_single(msg_ptr, flag_out_edges);
        pack_single(msg_ptr, addr_ser);

        for (auto &u : updates) {
            pack_single(msg_ptr, u);
        }

        #ifdef CONFIG_PREPARE_SEND
        msg.send();
        #else
        req.send(msg, msg_size);
        delete [] msg;
        #endif
    }

    // Save the confirmations required, and if there are none left then
    // progress to the ready state
    update_acks_needed_ += updates_to_send.size();
    if (update_acks_needed_ == 0)
        done_waiting_ready_nv_ne();

    debug_agent_(addr_ser, "SENDOUT | want acks:", update_acks_needed_);
}

void Agent::clear_batch_mem() {
    // Remove all leftover iteration state
    vn_.clear();
    vn_wait_.clear();
    vn_count_ = 0;
    vn_remaining_.clear();

    dormant_.clear();
    num_dormant_ = 0;

    #if defined(CONFIG_BSP) || defined(CONFIG_LBSP)
    it_ = -1;
    agent_msgs_needed_.clear();
    #endif

    num_inactive_ = 0;
    // Reset the state per the alg
    info_agent_(addr_ser, "RESET   |");
    for (auto & [v, vs] : graph_) {
        alg_.reset_state(vs);
        vs.replica_storage.clear();
        if (vs.local.state == INACTIVE)
            ++num_inactive_;
        #if !defined(CONFIG_BSP) && !defined(CONFIG_LBSP)
        // Prepare active for the next run
        if (vs.local.state == ACTIVE) {
            active_.insert(v);
        } else if (vs.local.state == DORMANT)
            throw std::runtime_error("State cannot be dormant outside of a batch");
        #endif
    }

    requested_leave_idle_ = false;
}

void Agent::gc() {
    #ifdef CONFIG_GC
    for (int32_t i = 0; i < (int32_t)vn_count_ && i < it_; ++i) {
        if (vn_remaining_[i] == 0) {
            debug_agent_(addr_ser, "GC      | ", i);
            #ifdef CONFIG_BSP
            vn_[i].clear();
            #endif
            vn_wait_[i].clear();
        }
    }
    #endif
}

void Agent::balance_va() {
    if (nE_ > 5*global_nE_/num_agents_/4) {
        char data[pack_msg_agent_size];
        char *data_ptr = data;
        pack_msg_agent(data_ptr, AGENT_LEAVE, addr_ser, vagent_count_);
        d_req_.send(data, sizeof(data));

        // Find the number of vagents to keep the highest degree
        size_t max_deg = 0;
        vertex_t arg_max_deg = (vertex_t)-1;
        for (auto& [v, ve] : graph_) {
            size_t deg = ve.in_neighbors.size()+ve.out_neighbors.size();
            if (deg > max_deg) {
                max_deg = deg;
                arg_max_deg = v;
            }
        }
        info_agent_(addr_ser, " max deg=", max_deg, " v=", arg_max_deg);

        // Find which vagent it corresponds to
        aid_t highest_vagent = 0;
        for (vertex_t n : graph_[arg_max_deg].in_neighbors) {
            bool dummy;
            edge_t e;
            e.src = n;
            e.dst = arg_max_deg;
            uint64_t dest = find_agent(e, IN, true, 0, dummy, true);
            uint64_t agent_ser;
            aid_t aid;
            unpack_agent(dest, agent_ser, aid);
            if (aid > highest_vagent) highest_vagent = aid;
        }
        for (vertex_t n : graph_[arg_max_deg].out_neighbors) {
            bool dummy;
            edge_t e;
            e.src = arg_max_deg;
            e.dst = n;
            uint64_t dest = find_agent(e, OUT, true, 0, dummy, true);
            uint64_t agent_ser;
            aid_t aid;
            unpack_agent(dest, agent_ser, aid);
            if (aid > highest_vagent) highest_vagent = aid;
        }

        ++highest_vagent;
        vagent_count_ = std::min(vagent_count_, highest_vagent);

        data_ptr = data;
        pack_msg_agent(data_ptr, AGENT_JOIN, addr_ser, vagent_count_);
        d_req_.send(data, sizeof(data));

        info_agent_(addr_ser, "VA UPDT | ", vagent_count_, " ", highest_vagent);
    } else
        info_agent_(addr_ser, "VA UPDT | no change");
    state_ = WAIT_FOR_LB;
}
