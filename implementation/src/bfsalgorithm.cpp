/**
 * ElGA BFS Algorithm Implementation
 *
 * Authors: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "bfsalgorithm.hpp"

#include <algorithm>

void BFSAlgorithm::run(VertexStorage &v,
        size_t nV,
        vn_t &vn,
        vnw_t &vnw,
        vnr_t &vnr,
        VertexNotification &vertex_notification, bool &notify_out, bool &notify_in, bool &notify_replica) {
    auto &ls = v.local;
    auto &in_neighbors = v.in_neighbors;
    auto &out_neighbors = v.out_neighbors;
    auto &replica_storage = v.replica_storage;

    vertex_t new_dist = std::numeric_limits<vertex_t>::max();

    if (ls.iteration == 0) {
        // Initialize dist
        if (v.vertex == start_)
            new_dist = 0;
        else
            new_dist = std::numeric_limits<vertex_t>::max();
    } else {
        #ifdef CONFIG_SYM_BFS
        for (auto& n : out_neighbors) {
            #ifdef RUNTIME_CHECKS
            if (vn.count(n) == 0) throw std::runtime_error("No neighbor value");
            #endif
            if (vn[n].dist < new_dist) new_dist = vn[n].dist;
        }
        #endif
        for (auto& n : in_neighbors) {
            #ifdef RUNTIME_CHECKS
            if (vn.count(n) == 0) throw std::runtime_error("No neighbor value");
            #endif
            if (vn[n].dist < new_dist) new_dist = vn[n].dist;
        }
        if (ls.rep_dist < new_dist) new_dist = ls.rep_dist;
    }
    if (new_dist < ls.dist) {
        ls.dist = new_dist;
        ls.rep_dist = new_dist;

        if (v.replicas.size() > 0) {
            replica_storage[ls.iteration+1][v.self].dist = new_dist;
            notify_replica = true;
        }

        notify_out = true;
        #ifdef CONFIG_SYM_BFS
        notify_in = true;
        #endif
        // Check for overflow
        if (ls.dist == std::numeric_limits<vertex_t>::max())
            vertex_notification.dist = ls.dist;
        else
            vertex_notification.dist = ls.dist+1;
    }

    ls.state = INACTIVE;

    ++ls.iteration;
}

void BFSAlgorithm::reset_state(VertexStorage &v) {
    v.local.iteration = 0;
    v.local.rep_dist = std::numeric_limits<vertex_t>::max();
    v.local.state = ACTIVE;
}
void BFSAlgorithm::reset_output(VertexStorage &v) {
    reset_state(v);
    v.local.dist = std::numeric_limits<vertex_t>::max();
}
void BFSAlgorithm::save(std::ofstream& of, VertexStorage &v) {
    of << v.vertex << " " << v.local.dist << '\n';
}
void BFSAlgorithm::dump_ovn_state(std::ofstream& of, vertex_t v, VertexNotification &ve) {
    of << " " << v << ":" << ve.dist;
}
void BFSAlgorithm::set_active(VertexStorage &v, VertexNotification &vn) {
    if (v.local.dist > vn.dist)
        v.local.state = ACTIVE;
}
void BFSAlgorithm::set_rep_active(VertexStorage &v, ReplicaLocalStorage &rv) {
    #ifdef DEBUG
    std::cerr << "set_rep_active() mine=" << v.local.dist << " new=" << rv.dist << std::endl;
    #endif
    if (v.local.dist > rv.dist) {
        v.local.rep_dist = rv.dist;
        #ifdef DEBUG
        std::cerr << "SET:: " << rv.dist << std::endl;
        #endif
        v.local.state = ACTIVE;
    }
}
