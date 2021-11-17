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

#include "directory.hpp"

#include "pack.hpp"

#include <thread>
#include <chrono>
#include <iostream>
#include <random>

#include <iomanip>
#include <mutex>

using namespace elga;

std::mutex d_mutex {};

template<typename ...Args>
inline __attribute__((always_inline))
void info_(uint64_t addr_ser, Args && ...args) {
    std::lock_guard<std::mutex> guard(d_mutex);

    std::cerr << "[ElGA : Directory : " << std::setw(2) << (addr_ser>>32) << "] ";
    (std::cerr << ... << args);
    std::cerr << std::endl;
}

template<typename ...Args>
inline __attribute__((always_inline))
void debug_(uint64_t addr_ser, Args && ...args) {
    #ifdef DEBUG
    std::lock_guard<std::mutex> guard(d_mutex);

    std::cerr << "[ElGA : Directory : " << std::setw(2) << (addr_ser>>32) << "] ";
    (std::cerr << ... << args);
    std::cerr << std::endl;
    #endif
}

namespace elga::directory {

    /** Helper function to print usage */
    void usage_() {
        std::cout << "Usage: directory parameter" << std::endl;
    }

    /** Helper function for printing out a help message */
    int help_() {
        std::cout << "\n"
            "The Directory service of ElGA.\n\n"
            "This is used to handle agents joining, leaving, and publishing\n"
            "updates to their counts and distributions. The Directories will\n"
            "connect to the Directory Master, register themselves, and\n"
            "internally share updates.\n\n"
            "Parameters:\n"
            "    help : display this message\n"
            "    ip-addr : the IP address to listen on\n"
            ;
        return EXIT_SUCCESS;
    }


    int main(int argc, const char **argv, const ZMQAddress &directory_master, localnum_t ln) {
        if (argc != 2) { usage_(); return help_(); }

        if (std::string(argv[1]) == "help") return help_();

        // Form our address
        ZMQAddress addr(argv[1], ln);

        // Create the directory server
        elga::Directory d(addr, directory_master);

        // Begin participating in comms
        d.join_directory();
        d.join_peers();

        // Start the actual server
        d.start();

        return 0;
    }

}

void Directory::join_directory() {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Directory] joining directory master pub" << std::endl;
    #endif
    sub(DIRECTORY_JOIN);
    sub(DIRECTORY_LEAVE);
    sub(AGENT_JOIN);
    sub(AGENT_LEAVE);
    sub(SHUTDOWN);
    sub(START);
    sub(SAVE);
    sub(DUMP);
    sub(HEARTBEAT);
    sub(READY_SYNC_INT);
    sub(HAVE_UPDATE);
    sub(READY_NV_NE_INT);
    #ifdef CONFIG_CS
    sub(CS_UPDATE);
    sub(CS_LB);
    #endif
    sub(UPDATE);
    sub(RESET);
    sub(CHK_T);
    sub(VA);
    #ifdef CONFIG_AUTOSCALE
    sub(AS_QUERY);
    #endif
    sub_connect(dm_);

    //wait_for_heartbeat

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Directory] registering" << std::endl;
    #endif

    ZMQRequester dm_req(dm_, addr_);
    // The registration message is:
    // <DIRECTORY_JOIN><addr>
    size_t size = sizeof(msg_type_t)+sizeof(uint64_t);
    char data[size];
    pack_nm_msg_uint64(data, DIRECTORY_JOIN, addr_.serialize());
    dm_req.send(data, size);
    dm_req.wait_ack();
}

void Directory::join_peer(uint64_t ser_addr) {
    if (ser_addr == addr_.serialize()) return;
    ZMQAddress peer(ser_addr);
    sub_connect(peer);

    // Add it to our directory list
    directories_.insert(ser_addr);
}

void Directory::leave_peer(uint64_t ser_addr) {
    ZMQAddress peer(ser_addr);
    sub_disconnect(peer);

    // Add it to our directory list
    directories_.erase(ser_addr);
}

#ifdef CONFIG_AUTOSCALE
void Directory::autoscaler() {
    double rate = 0.;
    for (const auto & [id, r] : as_rate_) {
        rate += r;
    }

    scale_direction dir;
    time_t now = time(NULL);

    size_t target = rate / (double)AUTOSCALE_QR_TARGET + 1;
    if (target > AUTOSCALE_MAX_AGENTS)
        target = AUTOSCALE_MAX_AGENTS;
    if (target > agents_.size() + dead_agents_.size())
        target = agents_.size() + dead_agents_.size();
    if (target < AUTOSCALE_MIN_AGENTS)
        target = AUTOSCALE_MIN_AGENTS;

    printf("T,%lu,%lf,%ld,%ld,%ld,%ld,%d", now, rate, agents_.size(), dead_agents_.size(), target, as_req_, as_wait_);
    std::cout << std::endl;

    if (target == agents_.size() && as_wait_ == 0)
        return;

    if (as_wait_ > 0) {
        --as_wait_;
        return;
    } else if (as_wait_ < -1) {
        ++as_wait_;
        return;
    }

    if (agents_.size() == 0) return;

    if (as_wait_ == 0) {
        as_wait_ = -AUTOSCALE_EMA;
        return;
    }

    as_wait_ = AUTOSCALE_EMA*2;

    size_t num_to_scale;

    // Determine if we need to go up or down
    if (target < agents_.size()) {
        dir = SCALE_IN;
        num_to_scale = agents_.size()-target;
    } else {
        dir = SCALE_OUT;
        num_to_scale = target-agents_.size();
    }
    as_req_ = target;

    // Pick agents to change
    std::random_device rd;
    std::mt19937 mt(rd());

    std::vector<uint64_t> agents;

    if (dir == SCALE_IN)
        std::copy(std::begin(agents_), std::end(agents_), std::back_inserter(agents));
    else if (dir == SCALE_OUT)
        std::copy(std::begin(dead_agents_), std::end(dead_agents_), std::back_inserter(agents));

    std::shuffle(std::begin(agents), std::end(agents), mt);

    #ifdef AUTOSCALE_LIMIT_CHANGES
    num_to_scale = std::min(num_to_scale, agents.size()/2);
    #endif

    info_(addr_ser, "SCALE: ", (dir == SCALE_IN) ? '-' : '+', num_to_scale);
    size_t msg_size = sizeof(msg_type_t) + sizeof(scale_direction) + sizeof(uint64_t)*num_to_scale;
    char* msg = new char[msg_size];
    char* msg_ptr = msg;
    pack_msg(msg_ptr, AS_SCALE);
    pack_single(msg_ptr, dir);
    for (size_t idx = 0; idx < num_to_scale; ++idx)
        pack_single(msg_ptr, agents[idx]);

    pub(msg, msg_size);

    delete[] msg;
}
#endif

bool Directory::heartbeat() {
    if (ZMQChatterbox::heartbeat() == false)
        return false;

    #ifdef CONFIG_AUTOSCALE
    autoscaler();
    #endif

    if (!notify_)
        return true;

    // Broadcast the agent list and sketch
    #ifdef CONFIG_CS
    size_t cms_size = CountMinSketch::size();
    #else
    size_t cms_size = 0;
    #endif
    size_t size = sizeof(msg_type_t)+sizeof(uint8_t)+agents_.size()*sizeof(uint64_t)+cms_size;
    ZMQMessage msg = prepare_pub(size);

    // Get the raw destination space
    char *data = msg.edit_data();

    pack_msg(data, DIRECTORY_UPDATE);
    // Indicate if something changed
    uint8_t changed = 0;
    if (notify_changed_)
        changed = 1;
    pack_single(data, changed);
    // Now, add in the agents
    size_t ctr = 0;
    uint64_t *data_agents = (uint64_t*)data;
    for (auto agent : agents_)
        data_agents[ctr++] = agent;
    data += sizeof(uint64_t)*agents_.size();

    #ifdef CONFIG_CS
    // Finally, include the count sketch
    char* cms_ser = cms_.serialize();
    memcpy(data, cms_ser, cms_size);
    data += cms_size;
    #endif

    if (notify_changed_)
        info_(addr_ser, "sent new directory, num agents: ", agents_.size());

    // Send out the message, and mark that we have notified
    msg.send();
    notify_ = false;
    notify_changed_ = false;

    return true;
}

void Directory::join_peers() {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Directory] finding peer directories" << std::endl;
    #endif
    // We have already joined the directory master pub, so we are okay
    // to query and not worry about missed joins after our query

    ZMQRequester dm_req(dm_, addr_);

    dm_req.send(GET_DIRECTORIES);
    ZMQMessage data = dm_req.read();

    size_t total_size = data.size();
    const char *raw_msg = data.data();

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Directory] joining directory pubs" << std::endl;
    #endif

    // Find ourselves
    uint64_t myself = addr_.serialize();

    // Iterate through and create each directory address
    for (size_t pos = sizeof(uint64_t); pos <= total_size; pos += sizeof(uint64_t)) {
        size_t rpos = pos-sizeof(uint64_t);
        uint64_t ser_addr = *(uint64_t*)(&raw_msg[rpos]);

        join_peer(ser_addr);
    }
}

void Directory::shutdown() {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Directory] initiating graceful, local shutdown" << std::endl;
    #endif

    // We will not process any more messages (no acks sent, etc.)
    // Inform the directory master
    char leavemsg[sizeof(msg_type_t)+sizeof(uint64_t)];

    pack_nm_msg_uint64(leavemsg, DIRECTORY_LEAVE, addr_.serialize());

    // First, de-register with the directory master
    ZMQRequester dm_req(dm_, addr_);
    dm_req.send(leavemsg, sizeof(leavemsg));
    dm_req.wait_ack();

    // Publish a note for everyone to move to other servers
    msg_type_t note = DISCONNECT;
    pub((char*)&note, sizeof(note));

    // Give a grace period to get the pub sent
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

bool Directory::agent_join(uint64_t *agent_list, size_t num_agents) {
    if (num_agents == 0) return false;

    // Determine if we have seen this before
    // Note a slight problem here if agents are flapping
    if (agents_.count(agent_list[0]) > 0) return false;

    // Insert the agents
    for (size_t ctr = 0; ctr < num_agents; ++ctr) {
        agents_.insert(agent_list[ctr]);
        #ifdef CONFIG_AUTOSCALE
        if (dead_agents_.count(agent_list[ctr]) > 0)
            dead_agents_.erase(agent_list[ctr]);
        #endif
    }

    // Then, mark we need to send out a directory update
    notify_ = true;
    notify_changed_ = true;

    debug_(addr_ser, "num agents: ", agents_.size());

    return true;
}

bool Directory::agent_leave(uint64_t *agent_list, size_t num_agents) {
    if (num_agents == 0) return false;

    if (agents_.count(agent_list[0]) == 0) return false;

    // Remove the agents
    for (size_t ctr = 0; ctr < num_agents; ++ctr) {
        agents_.erase(agent_list[ctr]);
        #ifdef CONFIG_AUTOSCALE
        as_rate_.erase(agent_list[ctr]);
        dead_agents_.insert(agent_list[ctr]);
        #endif
    }

    // Then, mark we need to send out a directory update
    notify_ = true;
    notify_changed_ = true;

    return true;
}

#ifdef CONFIG_CS
void Directory::cs_update(const char *data, size_t cs_size) {
    if (cs_size > 0) {
        // Merge the new CS with the main, full CS
        CountMinSketch new_cms {data};
        cms_.merge(new_cms);
    }

    ++cms_recv_;

    if (cms_recv_ >= agents_.size()) {
        notify_ = true;
        notify_changed_ = true;
        info_(addr_ser, "sending new CMS");
        cms_recv_ = 0;
    }
}
#endif

void Directory::start() {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Directory] running" << std::endl;
    #endif

    bool keep_running = true;
    while (keep_running) {
        // Check for our process shutdown
        if (global_shutdown) {
            shutdown();

            keep_running = false;
            break;
        }

        heartbeat();

        #ifdef DEBUG_VERBOSE
        std::cerr << "[ElGA : Directory] polling" << std::endl;
        #endif
        // Wait for a request
        for (auto sock : poll()) {
            // Retrieve the message to determine the type
            ZMQMessage msg(sock);

            size_t total_size = msg.size();

            if (total_size < sizeof(msg_type_t)) throw std::runtime_error("Message too small");

            // Read out the message type and data
            const char *data = msg.data();

            msg_type_t type = unpack_msg(data);

            #ifdef DEBUG_VERBOSE
            std::cerr << "[ElGA : Directory] got message: " << (int)type << std::endl;
            #endif

            switch (type) {
                case DIRECTORY_JOIN: {
                    // Connect to the new directory
                    // Unless it is us, of course
                    uint64_t ser_addr = *(uint64_t*)data;
                    join_peer(ser_addr);
                    break; }
                case DIRECTORY_LEAVE: {
                    uint64_t ser_addr = *(uint64_t*)data;
                    leave_peer(ser_addr);
                    break; }
                case AGENT_JOIN: {
                    size_t num_agents = (total_size-sizeof(msg_type_t))/sizeof(uint64_t);
                    if (agent_join((uint64_t*)data, num_agents))
                        pub(msg.data(), total_size);
                    break; }
                case AGENT_LEAVE: {
                    size_t num_agents = (total_size-sizeof(msg_type_t))/sizeof(uint64_t);
                    if (agent_leave((uint64_t*)data, num_agents))
                        pub(msg.data(), total_size);
                    break; }
                #ifdef CONFIG_AUTOSCALE
                case AS_QUERY: {
                    uint64_t recv_agent;
                    double val;
                    unpack_single(data, recv_agent);
                    unpack_single(data, val);

                    if (dead_agents_.count(recv_agent) > 0) break;

                    const double alpha = 2./(AUTOSCALE_EMA+1);
                    as_rate_[recv_agent] = alpha*val + (1-alpha)*as_rate_[recv_agent];

                    break;
                }
                #endif
                #ifdef CONFIG_CS
                case CS_UPDATE: {
                                    cs_update(data, total_size-sizeof(msg_type_t));
                                    break;
                                }
                #endif
                case NEED_DIRECTORY: {
                                         notify_ = true;
                                         break;
                                     }
                case READY_NV_NE_INT:
                case READY_NV_NE: {
                                      // First, the agent is ready
                                      ++ready_ctr_;

                                      // Second, the agent has provided
                                      // their changed nV and nE values,
                                      // so we need to update the totals
                                      // accordingly
                                      double unV;
                                      int64_t unE;
                                      unpack_unv_une(data, unV, unE);

                                      debug_(addr_ser, "got ", unV);
                                      nV_ += unV;
                                      nE_ += unE;

                                      // Update other directories with the
                                      // increase nV/nE
                                      if (type == READY_NV_NE) {
                                          char msg[pack_msg_unv_une_size];
                                          char *msg_ptr = msg;
                                          pack_msg_unv_une(msg_ptr, READY_NV_NE_INT, unV, unE);
                                          pub(msg, pack_msg_unv_une_size);
                                      }

                                      // Finally, if all agents are ready,
                                      // then the next phase can begin
                                      if (ready_ctr_ == agents_.size()) {
                                          // This will occur when we have
                                          // the sync ctr for all known
                                          // agents, not just our agents

                                          // Send out the NV/NE message,
                                          // which will begin the next
                                          // phase
                                          char msg[pack_msg_size_nv];
                                          char *msg_ptr = msg;
                                          pack_msg(msg_ptr, NV);
                                          pack_nv(msg_ptr, (size_t)nV_, nE_);
                                          pub(msg, sizeof(msg));

                                          // Reset the sync ctr
                                          ready_ctr_ -= agents_.size();
                                          it_ = 0;
                                          info_(addr_ser, "ready NV NE");
                                      }
                                      break;
                                  }
                case SHUTDOWN: {
                                   if (!keep_running) break;
                                   // If we receive a shutdown request, bring the whole
                                   // system down--broadcast it out
                                   msg_type_t sd = SHUTDOWN;
                                   pub((char*)&sd, sizeof(sd));
                                   // Stop the server, no need for cleanup
                                   keep_running = false;
                                   std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                   break;
                               }
                #ifdef CONFIG_CS
                case CS_LB:
                #endif
                case RESET:
                case CHK_T:
                case VA:
                case UPDATE:
                case SAVE:
                case DUMP:
                case START: {
                                // Re-broadcast
                                msg_type_t st = type+DO_ADD;  // Move to the DO_ variant
                                char new_msg[total_size];
                                char* new_msg_ptr = new_msg;
                                pack_msg(new_msg_ptr, st);
                                memcpy(new_msg_ptr, data, total_size-sizeof(msg_type_t));
                                pub(new_msg, total_size);
                                break;
                            }
                case HEARTBEAT:
                    break;
                case SIMPLE_SYNC: {
                                    // Re-broadcast
                                    pub(msg.data(), total_size);

                                    // Increment the sync counter
                                    if (++simple_sync_ >= agents_.size()) {
                                        info_(addr_ser, "simple sync: ", simple_sync_);
                                        simple_sync_ = 0;
                                        msg_type_t st = SIMPLE_SYNC_DONE;
                                        pub((const char*)&st, sizeof(st));

                                        notify_ = true;
                                        notify_changed_ = true;
                                    }
                                    break;
                                  }
                case READY_SYNC_INT:
                case READY_SYNC: {
                                     size_t this_dormant;
                                     unpack_single(data, this_dormant);

                                     // Increment the sync counter
                                     it_t msg_it = it_;
                                     it_t msg_batch = batch_;
                                     if (type == READY_SYNC_INT) {
                                        unpack_single(data, msg_it);
                                        unpack_single(data, msg_batch);
                                     }
                                     ++sync_ctr_[msg_batch][msg_it];

                                     num_dormant_[msg_batch][msg_it] += this_dormant;

                                     // Broadcast this
                                     if (type == READY_SYNC) {
                                         debug_(addr_ser, "re-broadcast READY_SYNC, my ctr=", sync_ctr_[batch_][it_]);
                                         size_t msg_size = sizeof(msg_type_t)+sizeof(size_t)+sizeof(it_t)+sizeof(batch_t);
                                         char new_msg[msg_size];
                                         char *msg_ptr = new_msg;

                                         pack_msg(msg_ptr, READY_SYNC_INT);
                                         pack_single(msg_ptr, this_dormant);
                                         pack_single(msg_ptr, it_);
                                         pack_single(msg_ptr, batch_);

                                         pub(new_msg, msg_size);
                                     } else {
                                         debug_(addr_ser, "received READY_SYNC_INC[", msg_batch, "][", msg_it, "] (I am ", it_, ") my ctr=", sync_ctr_[batch_][it_]);
                                     }
                                     debug_(addr_ser, "at READY_SYNC_INC[", msg_batch, "][", msg_it, "] (I am ", it_, ") my ctr=", sync_ctr_[batch_][it_]);

                                     // If the counter is a full sync,
                                     // broadcast that and reset
                                     if (sync_ctr_[batch_][it_] == agents_.size()) {
                                         char data[sizeof(msg_type_t)+sizeof(size_t)];
                                         char *data_ptr = data;

                                         pack_msg(data_ptr, SYNC);
                                         pack_single(data_ptr, num_dormant_[batch_][it_]);

                                         pub(data, sizeof(data));
                                         info_(addr_ser, "SENDING SYNC ", batch_, ":", it_);

                                         // If previously there were no
                                         // remaining active vertices,
                                         // then increment our batch ID,
                                         // preventing lost prior messages
                                         // from triggering a new batch
                                         // continuation
                                         if (num_dormant_[batch_][it_] == 0)
                                            ++batch_;

                                         // Increment the iteration
                                         ++it_;

                                         // Reset the counters
                                         agents_idle_ = true;
                                     } else if (sync_ctr_[batch_][it_] > agents_.size())
                                        throw std::runtime_error("Received too many syncs");
                                     break;
                                 }
                    case HAVE_UPDATE: {
                                          // Broadcast if:
                                          //    -agents are idle (haven't
                                          //    already broadcasted)
                                          //    -batch id is the same
                                          batch_t batch_of_req;
                                          unpack_batch(data, batch_of_req);
                                          if (batch_of_req < batch_)
                                              break;
                                          if (batch_of_req > batch_)
                                              throw std::runtime_error("Future batch received");
                                          if (!agents_idle_) // Keep from re-sending this
                                              break;

                                          // Broadcast it
                                          pub(msg.data(), total_size);

                                          // Now, agents are no longer
                                          // idle
                                          agents_idle_ = false;
                                          break;
                                      }
                default:
                    info_(addr_ser, "ERROR : received type ", (int)type);
                    throw std::runtime_error("Unknown message");
            }
        }
    }

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Directory] stopping" << std::endl;
    #endif
}
