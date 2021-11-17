/**
 * ElGA Page Rank Algorithm Implementation
 *
 * Authors: Kaan Sancak, Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "pralgorithm.hpp"

void PageRankAlgorithm::run(VertexStorage &v,
        size_t nV,
        vn_t &vn,
        vnw_t &vnw,
        vnr_t &vnr,
        VertexNotification &vertex_notification, bool &notify_out, bool &notify_in, bool &notify_replica) {
    auto &local_storage = v.local;
    auto &in_neighbors = v.in_neighbors;
    auto &out_neighbors = v.out_neighbors;
    auto &replica_storage = v.replica_storage;

    // Convert storage objects
    PRLocalStorage *pr_ls = static_cast<PRLocalStorage *>(&local_storage);

    if (pr_ls->iteration == 0) {
        pr_ls->pr = 1.0f / nV;
    }

    if (pr_ls->iteration > PAGERANK_SUPERSTEPS) {
        // Computation is completely over
        ++pr_ls->iteration;
        pr_ls->state = INACTIVE;
        return;
    }

    pr_t new_pr = 0.;

    it_t cur_it = pr_ls->iteration;
    if (v.replicas.size() == 0 ||
            replica_storage[cur_it].size() != v.replicas.size()) {
        // Read all neighbors
        if (cur_it > 0) {
            for (const auto &e : in_neighbors) {
                #ifdef RUNTIME_CHECKS
                if (vn[cur_it].count(e) == 0) throw std::runtime_error("No neighbor: me=" + std::to_string(v.vertex) +  " ngh=" + std::to_string(e) + " it=" + std::to_string(cur_it));
                #endif
                new_pr += vn[cur_it][e].scaled_pr;
            }
        }
        pr_ls->out_degree = out_neighbors.size();
        // Set replica storage if necessary
        if (replica_storage[cur_it].size() != v.replicas.size()) {
            replica_storage[cur_it][v.self].pr = new_pr;
            replica_storage[cur_it][v.self].out_degree = pr_ls->out_degree;
            pr_ls->state = REPWAIT;
            notify_replica = true;
            return;
        }
    } else {
        // Load state from replicas
        pr_ls->out_degree = 0;
        for (auto & [agent_ser, rep] : replica_storage[cur_it]) {
            if (cur_it > 0)
                new_pr += rep.pr;
            pr_ls->out_degree += rep.out_degree;
        }
    }

    // Calculate new page rank
    new_pr = (1.0f - DAMPING_FACTOR)/nV + DAMPING_FACTOR * new_pr;

    it_t next_it = ++pr_ls->iteration;

    // Update the pr
    if (next_it > 1)
        pr_ls->pr = new_pr;

    // Now, update our pr and propagate it fully, to every out neighbor
    // That is, do a vertex-level notification
    pr_t scaled_pr = pr_ls->pr / pr_ls->out_degree;

    vertex_notification.scaled_pr = scaled_pr;

    notify_out = true;

    // Now, set the PR for the next iteration
    if (next_it == 1)
        pr_ls->pr = new_pr;

    pr_ls->state = DORMANT;
};

void PageRankAlgorithm::reset_state(VertexStorage &v) {
    // This is not a batch algorithm, so we want to actually completely
    // reset state, and make everyone active
    auto &ls = v.local;
    ls.state = ACTIVE;
    ls.iteration = 0;
    ls.vertex_recv_needed = 0;
    ls.neighbor_recv_needed = 0;
    ls.replica_recv_needed = 0;
    ls.out_degree = 0;
}
void PageRankAlgorithm::reset_output(VertexStorage &v) {
    auto &ls = v.local;
    ls.pr = 0.0;
}
void PageRankAlgorithm::save(std::ofstream& of, VertexStorage &v) {
    of << v.vertex << " " << v.local.pr << "\n";
}
void PageRankAlgorithm::dump_ovn_state(std::ofstream& of, vertex_t v, VertexNotification &ve) {
    of << " " << v << ":" << ve.scaled_pr;
}
