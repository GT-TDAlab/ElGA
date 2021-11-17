/**
 * ElGA LPA Algorithm Implementation
 *
 * Authors: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "lpaalgorithm.hpp"
#include <unordered_map>

void LPAAlgorithm::run(VertexStorage &v,
        size_t nV,
        vn_t &vn,
        vnw_t &vnw,
        vnr_t &vnr,
        VertexNotification &vertex_notification, bool &notify_out, bool &notify_in, bool &notify_replica) {
    auto &ls = v.local;
    auto &in_neighbors = v.in_neighbors;
    auto &out_neighbors = v.out_neighbors;
    auto &replica_storage = v.replica_storage;

    if (v.replicas.size() > 0)
        throw std::runtime_error("Not yet implemented");

    if (ls.iteration == 0) {
        ls.lp = v.vertex;
    }

    std::unordered_map<vertex_t, size_t> freq;
    for (const auto &e : in_neighbors) {
        if (vn.count(e) == 0) {
            vn[e].v = e;
            vn[e].lp = e;
        }
        ++freq[vn[e].lp];
    }
    for (const auto &e : out_neighbors) {
        if (vn.count(e) == 0) {
            vn[e].v = e;
            vn[e].lp = e;
        }
        ++freq[vn[e].lp];
    }

    size_t max_freq = 0;
    vertex_t new_lp = ls.lp;
    for (auto & [lab, cnt] : freq) {
        if (cnt > max_freq) {
            max_freq = cnt;
            new_lp = lab;
        } else if (cnt == max_freq && lab < new_lp)
            new_lp = lab;
    }

    it_t next_it = ++ls.iteration;

    if (new_lp != ls.lp || ls.iteration == 1) {
        #ifndef DEBUG_EXCESSIVE
        std::cerr << "UPDATE " << v.vertex << ": " << ls.lp << "->" << new_lp << std::endl;
        #endif
        ls.lp = new_lp;
        notify_out = true;
        notify_in = true;

        vertex_notification.lp = ls.lp;
    }
    ls.state = INACTIVE;
};

void LPAAlgorithm::reset_state(VertexStorage &v) {
    auto &ls = v.local;
    ls.iteration = 1;
}
void LPAAlgorithm::reset_output(VertexStorage &v) {
    auto &ls = v.local;
    ls.lp = std::numeric_limits<vertex_t>::max();
    ls.iteration = 0;
    ls.state = ACTIVE;
}
void LPAAlgorithm::save(std::ofstream& of, VertexStorage &v) {
    of << v.vertex << " " << v.local.lp << "\n";
}
void LPAAlgorithm::dump_ovn_state(std::ofstream& of, vertex_t v, VertexNotification &ve) {
    of << " " << v << ":" << ve.lp;
}
void LPAAlgorithm::set_active(VertexStorage &v, VertexNotification &vn) {
    v.local.state = ACTIVE;
}
void LPAAlgorithm::set_rep_active(VertexStorage &v, ReplicaLocalStorage &rv) {
    throw std::runtime_error("Not yet implemented");
}
