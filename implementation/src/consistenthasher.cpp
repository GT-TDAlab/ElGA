/**
 * ElGA consistent hasher
 *
 * Author: Kaan Sancak, Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "consistenthasher.hpp"

#include <random>

ConsistentHasher::ConsistentHasher(std::vector<uint64_t> &agents,
        ReplicationMap &rm) :
                agents_(), rm_(rm), ring_(), agent_map_() {

    update_agents(agents);
}

std::vector<uint64_t> ConsistentHasher::find(uint64_t key) {
    int64_t ring_size = ring_.size();
    std::vector<uint64_t> response;

    if (ring_size == 0) return response;

    int32_t replication = rm_.query(key);

    response.reserve(replication);

    int h = ring_size - 1;
    int l = 0;
    auto hkey = hashing::hash(key);
    // Find left-most insertion point
    while (l < h) {
        int mid = (l + h) >> 1;
        if (ring_[mid] >= hkey)
            h = mid;
        else
            l = mid + 1;
    }

    if (l + replication < ring_size)
        std::copy(ring_.begin() + l, ring_.begin() + l + replication, std::back_inserter(response));
    else {
        std::copy(ring_.begin() + l, ring_.end(), std::back_inserter(response));
        std::copy(ring_.begin(), ring_.begin() + std::abs(replication - ring_size + l), std::back_inserter(response));
    }

    // Get the idx of each response elements
    for (uint64_t &key : response)
        key = agent_map_[key];

    return response;
}

uint64_t ConsistentHasher::find_one(uint64_t key, uint64_t owner_check, bool &have_ownership) {
    std::vector<uint64_t> containers = find(key);

    have_ownership = false;

    if (containers.size() == 0) return 0;

    if (owner_check != 0) {
        for (auto c : containers) {
            if ((c & ((1llu<<49)-1)) == owner_check) {
                have_ownership = true;
                break;
            }
        }
    }

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<size_t> dist(0, containers.size()-1);

    return containers[dist(mt)];
}

void ConsistentHasher::update_agents(std::vector<uint64_t> &agents) {
    // This might be done in a better way
    ring_.clear();
    ring_.reserve(agents.size());
    agent_map_.clear();

    for(uint64_t agent: agents) {
        uint64_t h = hashing::hash(agent);
        ring_.push_back(h);
        agent_map_[h] = agent;
    }
    std::sort(ring_.begin(), ring_.end());

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ConsistentHasher] ring:";
    for (auto r : ring_)
        std::cerr << " " << r;
    std::cerr << std::endl;
    #endif
}
