/**
 * ElGA Page Rank Algorithm Class
 *
 * Authors: Kaan Sancak, Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef PR_ALGORITHM_HPP_
#define PR_ALGORITHM_HPP_
#define CONFIG_BSP

#include "types.hpp"

typedef double pr_t;

#include <iostream>
#include <fstream>
#include <cmath>

#include <unordered_map>
#include <unordered_set>
#include "absl/container/flat_hash_map.h"
#include <numeric>

class PRLocalStorage {
    public:
        pr_t pr;
        it_t iteration;
        vertex_t out_degree;
        local_state state;
        vertex_t vertex_recv_needed;
        vertex_t neighbor_recv_needed;
        uint16_t replica_recv_needed;
        PRLocalStorage() : pr(0.0), iteration(0), out_degree(0), state(ACTIVE),
            neighbor_recv_needed(0), replica_recv_needed(0) { }
};

class PRReplicaLocalStorage {
    public:
        pr_t pr;
        vertex_t out_degree;
        PRReplicaLocalStorage() : pr(0.0), out_degree(0) { }
};

class PRVertexNotification {
    public:
        vertex_t v;
        #ifndef CONFIG_BSP
        it_t it;
        #endif
        #ifdef CONFIG_NOTIFY_AGG
        vertex_t n;
        #endif
        pr_t scaled_pr;
        PRVertexNotification() : v((vertex_t)-1),
                #ifndef CONFIG_BSP
                it(0),
                #endif
                scaled_pr(INFINITY) { }
};

using LocalStorage = PRLocalStorage;
using ReplicaLocalStorage = PRReplicaLocalStorage;
using VertexNotification = PRVertexNotification;

using vn_t = std::vector<absl::flat_hash_map<vertex_t, VertexNotification> >;
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
    VertexStorage() : vertex(-1) { }
} VertexStorage;

class PageRankAlgorithm {
    const pr_t DAMPING_FACTOR = 0.85;
    const pr_t EPSILON = 1e-9;
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
        size_t const query_resp_size() { return sizeof(pr_t); }
        void const query(char* d, VertexStorage &v) { *(pr_t*)d = v.local.pr; }
        void const query(char* d) { *(pr_t*)d = INFINITY; }
};

using Algorithm = PageRankAlgorithm;

#endif
