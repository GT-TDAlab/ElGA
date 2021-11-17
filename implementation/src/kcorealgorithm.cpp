/**
 * ElGA k-Core Algorithm Implementation
 *
 * Authors: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "kcorealgorithm.hpp"

#include <algorithm>

void KCoreAlgorithm::run(VertexStorage &v,
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
        // Initialize tau to degree
        ls.tau = out_neighbors.size()+in_neighbors.size();
    } else {
        // Set tau based on the h-index of neighbors
        std::vector<vertex_t> taus;
        taus.reserve(out_neighbors.size()+in_neighbors.size());

        for (auto& n : in_neighbors) {
            if (vn.count(n) == 0) throw std::runtime_error("No neighbor value");
            taus.push_back(vn[n].tau);
        }
        for (auto& n : out_neighbors) {
            if (vn.count(n) == 0) throw std::runtime_error("No neighbor value");
            taus.push_back(vn[n].tau);
        }

        // Sort the neighbors, and compute the "H-index"
        std::sort(taus.begin(), taus.end(), std::greater<>());
        vertex_t new_tau = 0;
        size_t neighbors_size = taus.size();
        while (new_tau < neighbors_size) {
            if (taus[new_tau] > new_tau)
                new_tau++;
            else break;
        }

        if (ls.tau > new_tau) ls.tau = new_tau;
        else if (ls.tau < new_tau) throw std::runtime_error("tau increasing");
        else ls.state = INACTIVE;
    }

    if (ls.state != INACTIVE) {
        notify_out = true;
        notify_in = true;
        vertex_notification.tau = ls.tau;
    }

    ++ls.iteration;
}

void KCoreAlgorithm::reset_state(VertexStorage &v) {
    v.local.iteration = 0;
}
void KCoreAlgorithm::reset_output(VertexStorage &v) {
    v.local.tau = std::numeric_limits<vertex_t>::max();
    v.local.state = ACTIVE;
}
void KCoreAlgorithm::save(std::ofstream& of, VertexStorage &v) {
    of << v.vertex << " " << v.local.tau << '\n';
}
void KCoreAlgorithm::dump_ovn_state(std::ofstream& of, vertex_t v, VertexNotification &ve) {
    of << " " << v << ":" << ve.tau;
}
void KCoreAlgorithm::set_active(VertexStorage &v, VertexNotification &vn) {
    if (v.local.tau > vn.tau)
        v.local.state = ACTIVE;
}
void KCoreAlgorithm::set_rep_active(VertexStorage &v, ReplicaLocalStorage &rv) {
}
