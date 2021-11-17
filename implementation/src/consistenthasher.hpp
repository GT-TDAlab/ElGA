/**
 * ElGA Consistent Hasher Class
 *
 * Author: Kaan Sancak, Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef CONSISTENT_HASHER_HPP
#define CONSISTENT_HASHER_HPP

#include <algorithm>
#include <cmath>
#include <iostream>

#include <vector>
#include "absl/container/flat_hash_map.h"

#include "replicationmap.hpp"
#include "integer_hash.hpp"

class ConsistentHasher {
    private:
        std::vector<uint64_t> agents_;
        const ReplicationMap &rm_;
        std::vector<uint64_t> ring_;
        absl::flat_hash_map<uint64_t, uint64_t> agent_map_;

    public:
        ConsistentHasher(std::vector<uint64_t> &agents, ReplicationMap &rm);

        /** Return the number of replicas for a given key */
        int32_t count_reps(uint64_t key) { return rm_.query(key); }

        /** Retrieve all of the containers for a given key */
        std::vector<uint64_t> find(uint64_t key);

        /** Retrieve a single u.r. container in the consistent hash ring */
        uint64_t find_one(uint64_t key, uint64_t owner_check, bool &have_ownership);

        /** Support replacing the agents */
        void update_agents(std::vector<uint64_t> &agents);
};

#endif
