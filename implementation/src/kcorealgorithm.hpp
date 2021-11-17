/**
 * ElGA k-Core Algorithm Class
 *
 * Authors: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef KCORE_ALGORITHM_HPP_
#define KCORE_ALGORITHM_HPP_
#define CONFIG_LBSP

#include "types.hpp"

#include <iostream>
#include <fstream>
#include <cmath>

#include <limits>
#include <unordered_map>
#include <unordered_set>
#include "absl/container/flat_hash_map.h"
#include <numeric>

class KCoreLocalStorage {
    public:
        vertex_t tau;
        it_t iteration;
        local_state state;
        KCoreLocalStorage() : tau(std::numeric_limits<vertex_t>::max()),
            iteration(0), state(ACTIVE)
            { }
};

class KCoreReplicaLocalStorage {
    public:
        vertex_t tau;
        bool make_active;
        KCoreReplicaLocalStorage() : tau(std::numeric_limits<vertex_t>::max()), make_active(false) { }
};

class KCoreVertexNotification {
    public:
        vertex_t v;
        vertex_t tau;
        KCoreVertexNotification() : v(std::numeric_limits<vertex_t>::max()),
                tau(std::numeric_limits<vertex_t>::max()) { }
};

using LocalStorage = KCoreLocalStorage;
using ReplicaLocalStorage = KCoreReplicaLocalStorage;
using VertexNotification = KCoreVertexNotification;

using vn_t = absl::flat_hash_map<vertex_t, VertexNotification>;
using vnw_t = std::vector<absl::flat_hash_map<vertex_t, std::vector<std::pair<vertex_t, bool> > > >;
using vnr_t = std::vector<size_t>;

typedef struct VertexStorage {
    vertex_t vertex;
    LocalStorage local;
    std::unordered_set<uint64_t> replicas;
    uint64_t self;
    std::vector<vertex_t> in_neighbors;
    std::vector<vertex_t> out_neighbors;
    std::unordered_map<it_t, std::unordered_map<uint64_t, ReplicaLocalStorage>> replica_storage;
    VertexStorage() : vertex(std::numeric_limits<vertex_t>::max()) { }
} VertexStorage;

class KCoreAlgorithm {
    public:
        void run(VertexStorage &v,
                size_t nV,
                vn_t &vn,
                vnw_t &vnw,
                vnr_t &vnr,
                VertexNotification &vertex_notification, bool &notify_out, bool &notify_in, bool &notify_replica);
        void reset_state(VertexStorage &v);
        void reset_output(VertexStorage &v);
        void save(std::ofstream &of, VertexStorage &v);
        void dump_ovn_state(std::ofstream& of, vertex_t v, VertexNotification &ve);
        void set_active(VertexStorage &v, VertexNotification &vn);
        void set_rep_active(VertexStorage &v, ReplicaLocalStorage &rv);
        bool skip_rep_wait() const { return true; }
        size_t const query_resp_size() { return sizeof(vertex_t); }
        void const query(char* d, VertexStorage &v) { *(vertex_t*)d = v.local.tau; }
        void const query(char* d) { *(vertex_t*)d = 0; }
};

using Algorithm = KCoreAlgorithm;

#endif
