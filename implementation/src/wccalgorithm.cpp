/**
 * ElGA WCC Algorithm Implementation
 *
 * Authors: Kaan Sancak, Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "wccalgorithm.hpp"

void WCCAlgorithm::run(VertexStorage &v,
        size_t nV,
        vn_t &vn,
        vnw_t &vnw,
        vnr_t &vnr,
        VertexNotification &vertex_notification, bool &notify_out, bool &notify_in, bool &notify_replica) {
    auto &ls = v.local;
    auto &in_neighbors = v.in_neighbors;
    auto &out_neighbors = v.out_neighbors;
    auto &replica_storage = v.replica_storage;

    if (ls.iteration == 0) {
        ls.cc = v.vertex;
    }

    #ifdef CONFIG_TACTIVATE
    vertex_t new_cc = std::min(ls.new_cc, ls.cc);
    #else
    vertex_t new_cc = ls.cc;
    for (const auto &e : in_neighbors) {
        if (vn.count(e) == 0) {
            vn[e].v = e;
            vn[e].cc = e;
        }
        if (vn[e].cc < new_cc) new_cc = vn[e].cc;
    }
    for (const auto &e : out_neighbors) {
        if (vn.count(e) == 0) {
            vn[e].v = e;
            vn[e].cc = e;
        }
        if (vn[e].cc < new_cc) new_cc = vn[e].cc;
    }
    #endif
    if (ls.rep_cc < new_cc) new_cc = ls.rep_cc;

    it_t next_it = ++ls.iteration;

    if (new_cc < ls.cc || ls.iteration == 1) {
        #ifdef DEBUG_EXCESSIVE
        std::cerr << "UPDATE " << v.vertex << ": " << ls.cc << "->" << new_cc << std::endl;
        #endif
        ls.cc = new_cc;
        ls.rep_cc = new_cc;
        notify_out = true;
        notify_in = true;
        if (v.replicas.size() > 0) {
            replica_storage[next_it][v.self].cc = new_cc;
            notify_replica = true;
        }

        vertex_notification.cc = ls.cc;
    }
    ls.state = INACTIVE;
};

void WCCAlgorithm::reset_state(VertexStorage &v) {
    auto &ls = v.local;
    ls.iteration = 1;
    ls.rep_cc = std::numeric_limits<vertex_t>::max();
    #ifdef CONFIG_TACTIVATE
    ls.new_cc = std::numeric_limits<vertex_t>::max();
    #endif
}
void WCCAlgorithm::reset_output(VertexStorage &v) {
    auto &ls = v.local;
    ls.cc = std::numeric_limits<vertex_t>::max();
    ls.rep_cc = std::numeric_limits<vertex_t>::max();
    ls.iteration = 0;
    ls.state = ACTIVE;
}
void WCCAlgorithm::save(std::ofstream& of, VertexStorage &v) {
    of << v.vertex << " " << v.local.cc << "\n";
}
void WCCAlgorithm::dump_ovn_state(std::ofstream& of, vertex_t v, VertexNotification &ve) {
    of << " " << v << ":" << ve.cc;
}
void WCCAlgorithm::set_active(VertexStorage &v, VertexNotification &vn) {
    if (v.local.cc > vn.cc)
        v.local.state = ACTIVE;
}
void WCCAlgorithm::set_rep_active(VertexStorage &v, ReplicaLocalStorage &rv) {
    if (v.local.cc > rv.cc) {
        v.local.rep_cc = rv.cc;
        v.local.state = ACTIVE;
    }
}
