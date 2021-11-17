/**
 * ElGA participant in the graph implementation
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include <iostream>

#include "pack.hpp"
#include "participant.hpp"

using namespace elga;

Participant::Participant(ZMQAddress addr, const ZMQAddress &dm, bool persist) :
        ZMQChatterbox(addr), directory_(), rm_(), agents_(),
        #ifdef CONFIG_USE_AGENT_CACHE
        agent_cache_(),
        #endif
        lru_(), lru_lookup_(),
        ready_(false), ch_(agents_, rm_), d_req_(),
        num_agents_(0), num_vagents_(0),
        #ifdef CONFIG_TIME_FIND_AGENTS
        find_agent_t("agent_find"),
        #endif
        working_(false) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Participant] querying for a directory" << std::endl;
    #endif
    // First, find a directory for us to use
    ZMQRequester dm_req(dm, addr_);

    // Ask for a directory address
    dm_req.send(GET_DIRECTORY);
    ZMQMessage res = dm_req.read();

    if (res.size() == 0) {
        std::cerr << "[ElGA : Participant] WARNING: trying to participate, but no directories" << std::endl;
        return;
    }

    directory_ = ZMQAddress(*(uint64_t*)res.data());
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Participant] joining directory: " << directory_.get_remote_pub_str() << std::endl;
    #endif

    // Now, establish this as our directory server
    // Meaning: sub to its updates and notify it to send an update
    sub(HEARTBEAT);
    sub(SHUTDOWN);
    char du_all[] = {DIRECTORY_UPDATE, 0x0};
    sub(du_all, sizeof(du_all));
    char du_changes[] = {DIRECTORY_UPDATE, 0x1};
    sub(du_changes, sizeof(du_changes));
    sub(DISCONNECT);
    sub_connect(directory_);

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Participant] asking for directory" << std::endl;
    #endif
    ZMQRequester d_req(directory_, addr_, PULL);
    d_req.send(NEED_DIRECTORY);

    if (persist)
        d_req_.swap(d_req);

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Participant] ready to begin" << std::endl;
    #endif
}

void Participant::start() {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Participant] running" << std::endl;
    #endif

    bool keep_running = true;
    while (keep_running) {
        // Check for our process shutdown
        if (global_shutdown) {
            keep_running = shutdown();

            if (!keep_running)
                break;
        }

        heartbeat();

        #ifdef DEBUG_VERBOSE
        std::cerr << "[ElGA : Participant] polling" << std::endl;
        #endif

        pre_poll();

        keep_running = do_poll();
    }

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Participant] stopping" << std::endl;
    #endif
}

bool Participant::do_poll(bool drain) {
    bool keep_running = true;
    // Wait for a request
    long timeout = drain ? 0 : 2500;
    auto socks = poll(timeout);
    if (drain && socks.size() == 0) return false;
    working_ = false;
    for (auto sock : socks) {
        // Retrieve the message to determine the type
        ZMQMessage msg(sock);

        size_t total_size = msg.size();

        if (total_size < sizeof(msg_type_t)) throw std::runtime_error("Message too small");

        // Read out the message type and data
        const char *data = msg.data();

        msg_type_t type = unpack_msg(data);

        #ifdef DEBUG_VERBOSE
        std::cerr << "[ElGA : Participant] got message: " << (int)type << std::endl;
        #endif

        switch (type) {
            case SHUTDOWN: {
                // If we receieve a shutdown, the system is going down
                // Stop the server, no need for cleanup
                keep_running = false;
                break; }
            case HEARTBEAT:
                break;
            case DIRECTORY_UPDATE: {
                uint8_t changed;
                unpack_single(data, changed);
                directory_update(data, total_size-sizeof(msg_type_t)-sizeof(uint8_t));
                if (changed != 0)
                    handle_directory_update();
                break; }
            case DISCONNECT: {
                throw std::runtime_error("Unimplemented");
                break; }
            default: {
                working_ = true;
                if (!handle_msg(sock, type, data, total_size-sizeof(msg_type_t)))
                    throw std::runtime_error("Unknown message");
                break; }
        }
    }
    return keep_running;
}

void Participant::directory_update(const char *data, size_t size) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Participant] received directory of size " << size << std::endl;
    #endif

    if (!ready_) {
        // Change our subscription to only get changed updates
        char du_all[] = {DIRECTORY_UPDATE, 0x0};
        unsub(du_all, sizeof(du_all));
    }

    num_agents_ = (size
        #ifdef CONFIG_CS
        - CountMinSketch::size()
        #endif
        )/sizeof(uint64_t) ;
    // Read the incoming directory and sketch
    std::vector<uint64_t> agents;

    uint64_t *new_agents = (uint64_t*)data;

    real_agents_.clear();

    for (size_t ctr = 0; ctr < num_agents_; ++ctr) {
        aid_t num_vagents;
        uint64_t agent_serial;
        unpack_agent(new_agents[ctr], agent_serial, num_vagents);
        real_agents_.push_back(agent_serial);
        for (aid_t va = 0; va < num_vagents; ++va)
            agents.push_back(pack_agent(agent_serial, va));
    }

    num_vagents_ = agents.size();

    // Now, replace the consistent hasher
    std::swap(agents, agents_);

    ch_.update_agents(agents_);

    #ifdef CONFIG_CS
    // Next, replace the sketch
    rm_.update(data+size-CountMinSketch::size());
    #endif

    ready_ = true;

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Participant] installed directory with " << num_agents_ << " agents (" << num_vagents_ << " virtual)" << std::endl;
    #endif
}

uint64_t Participant::find_agent(edge_t e, edge_type et, bool find_owner, uint64_t owner_check, bool &have_ownership, bool return_va) {
    #ifdef CONFIG_TIME_FIND_AGENTS
    find_agent_t.tick();
    #endif
    #ifdef CONFIG_USE_AGENT_CACHE
    auto search = agent_cache_.find({e, et, find_owner});
    if (search != agent_cache_.end()) {
        have_ownership = std::get<1>(search->second);
        #ifdef DEBUG_VERBOSE
        std::cerr << "[ElGA : Participant] FOUND: " << (size_t)std::hash<std::tuple<edge_t, edge_type, bool> >()(std::make_tuple(e, et, find_owner)) << " " << (int)(et == IN) << ":" << e.src<<"->"<<e.dst << " |-> " << (std::get<0>(search->second)>>32) << std::endl;
        #endif
        #ifdef CONFIG_TIME_FIND_AGENTS
        find_agent_t.tock();
        #endif
        return std::get<0>(search->second);
    }
    #endif

    vertex_t u = (et == OUT) ? e.src : e.dst;
    vertex_t v = (et == OUT) ? e.dst : e.src;
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Participant] searching for owner for " << u << " first " << (int)(et == IN) << ":" << e.src<<"->"<<e.dst << std::endl;
    #endif

    uint64_t dest;
    if (!find_owner) {
        // We want to use a uniform random query to load balance
        dest = ch_.find_one(u, owner_check, have_ownership);
    } else {
        auto dests = ch_.find(u);
        if (dests.size() == 1) {
            dest = dests[0];
        } else {
            // Find its position in the second ring
            NoReplication rm;
            ConsistentHasher ch(dests, rm);
            bool dummy;
            dest = ch.find_one(v, 0, dummy);
        }
        have_ownership = false;
    }

    if (return_va) return dest;

    uint64_t agent_ser;
    aid_t aid;
    unpack_agent(dest, agent_ser, aid);
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Participant] SETTING: " << (size_t)std::hash<std::tuple<edge_t, edge_type, bool> >()(std::make_tuple(e, et, find_owner)) << " " << (int)(et == IN) << ":" << e.src<<"->"<<e.dst << " |-> " << ZMQAddress(agent_ser).get_remote_str() << std::endl;
    #endif

    #ifdef CONFIG_USE_AGENT_CACHE
    std::tuple<uint64_t, bool> cache_item = {agent_ser, have_ownership};
    agent_cache_[std::make_tuple(e, et, find_owner)] = cache_item;
    #endif
    #ifdef CONFIG_TIME_FIND_AGENTS
    find_agent_t.retock();
    #endif
    return agent_ser;
}

ZMQRequester& Participant::get_requester(uint64_t agent_ser, bool use_buffering) {
    // Check if it already exists in the LRU
    auto lookup = lru_lookup_.find(agent_ser);
    if (lookup == lru_lookup_.end()) {
        #ifdef CONFIG_MAINTAIN_LRU
        // We need to evict the oldest from the list and create a new
        // requester, if we are at the limit
        if (lru_.size() == LRU_LIMIT) {
            l_req::iterator back = std::prev(lru_.end());
            uint64_t old_ser = back->addr();
            lru_.erase(back);
            lru_lookup_.erase(old_ser);
        }
        #endif
        // Now, add the new one
        lru_.emplace(lru_.begin(), ZMQAddress(agent_ser), addr_, PULL, use_buffering);
        l_req::iterator list_item = lru_.begin();
        lru_lookup_[agent_ser] = list_item;

        return *list_item;
    } else {
        l_req::iterator list_item = lookup->second;
        #ifdef CONFIG_MAINTAIN_LRU
        // This is recently used, so move it to the front
        lru_.splice(lru_.begin(), lru_, list_item);
        #endif

        return *list_item;
    }
}
