/**
 * ElGA Replication Map
 *
 * Authors: Kaan Sancak, Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef REPLICATION_MAP_HPP_
#define REPLICATION_MAP_HPP_

#include "countminsketch.hpp"

#include <algorithm>

const uint64_t REP_THRESH = CONFIG_REP_THRESH;

class ReplicationMap{
    public:
        virtual int32_t query(uint64_t key) const = 0;
        virtual int32_t sk_query(uint64_t key) const = 0;
};

class CMSReplicationMap : public CountMinSketch, public ReplicationMap {
    public:
        int32_t query(uint64_t key) const { return CountMinSketch::query(key) / REP_THRESH + 1; }
        int32_t sk_query(uint64_t key) const { return CountMinSketch::query(key); }
};

class CSReplicationMap : public ReplicationMap, public CountSketch {
    public:
        int32_t query(uint64_t key) const { return CountSketch::query(key) / REP_THRESH + 1; }
        int32_t sk_query(uint64_t key) const { return CountSketch::query(key); }
};

class NoReplication : public ReplicationMap {
    public:
        NoReplication() {}
        int32_t query(uint64_t key) const { return 1; }
        int32_t sk_query(uint64_t key) const { return 0; }
};

#endif
