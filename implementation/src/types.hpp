/**
 * Constants and types used throughout ElGA
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <stdexcept>
#include <stdlib.h>

#include <functional>
#include <tuple>

#include <signal.h>

#include <iostream>
#include <string>

#include "integer_hash.hpp"

/** Version information */
#define ELGA_MAJOR 1
#define ELGA_MINOR 0

/** Configurable types */
using msg_type_t = uint8_t;
using localnum_t = uint16_t;
using aid_t = uint16_t;

using vertex_t = uint64_t;
using timestamp_t = uint64_t;
using weight_t = double;
using batch_t = uint32_t;
using it_t = int32_t;

typedef enum local_state {
    /** Inactive vertices will not begin computation unless a neighbor
     * triggers them to become active */
    INACTIVE,
    /** Active vertices wish to be processed and will be if their wait
     * counts are zero */
    ACTIVE,
    /** Dormant vertices can become active if notified by neighbors but
     * otherwise will wait a global synchronization and then become
     * active */
    DORMANT,
    /** REPWAIT vertices are waiting on replicas */
    REPWAIT
} local_state;

#ifdef CONFIG_AUTOSCALE
typedef enum scale_direction {
    SCALE_IN,
    SCALE_OUT
} scale_direction;
#endif

typedef struct edge {
    vertex_t src;
    vertex_t dst;
    //timestamp_t timestamp;
    //weight_t weight;

    bool operator==(const edge &other) const {
        return (src == other.src && dst == other.dst);
    }
} edge_t;
typedef enum edge_type {
    IN, OUT
} edge_type;

typedef struct update {
    edge_t e;
    edge_type et;
    uint32_t insert;
    bool operator==(const struct update &other) const {
        return (e == other.e && et == other.et && insert == other.insert);
    }
} update_t;

namespace std {
    // From Boost: hash_combine_impl
    // https://www.boost.org/doc/libs/1_71_0/boost/container_hash/hash.hpp
	// Templated by https://stackoverflow.com/questions/7110301/generic-hash-for-tuples-in-unordered-map-unordered-set
    template <class T>
    inline void hash_combine(std::size_t& seed, T const& v)
    {
        seed ^= hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }

    template<>
    struct hash<edge_t> {
        std::size_t operator()(const edge_t &e) const {
            size_t res = 0x0;
            hash_combine(res, e.src);
            hash_combine(res, e.dst);
            return res;
        }
    };

    template<>
    struct hash<tuple<edge_t, edge_type, bool> > {
        std::size_t operator()(const tuple<edge_t, edge_type, bool> &t) const {
            const edge_t &e = get<0>(t);
            const edge_type &et = get<1>(t);
            const bool &b = get<2>(t);

            size_t res = 0x0;
            hash_combine(res, e.src);
            hash_combine(res, e.dst);
            hash_combine(res, (int)et);
            hash_combine(res, b);

            return res;
        }
    };

    template<>
    struct hash<update_t> {
        std::size_t operator()(const update_t &u) const {
            const edge_t &e = u.e;
            const edge_type &et = u.et;
            const bool &b = u.insert;

            size_t res = 0x0;
            hash_combine(res, e.src);
            hash_combine(res, e.dst);
            hash_combine(res, (int)et);
            hash_combine(res, b);

            return res;
        }
    };
}

/** Hard-coded directories */
#define QUOTE(s) #s
#define STR(s) QUOTE(s)
#ifndef CONFIG_SAVE_DIR
    #define CONFIG_SAVE_DIR /scratch/elga
#endif
#define SAVE_DIR_STR STR(CONFIG_SAVE_DIR)
const std::string SAVE_DIR = SAVE_DIR_STR;

/** Helper class to wrap and capture argument errors */
class arg_error : public ::std::runtime_error {
    public:
        template<typename T>
            arg_error(T t) : ::std::runtime_error(t) { }
};

// Note: these are explicitly integers, avoiding ambiguity with compiler
// specific enums
/** Constants used for protocols */
#define SHUTDOWN            0x01
#define GET_DIRECTORIES     0x02
#define GET_DIRECTORY       0x03
#define DIRECTORY_JOIN      0x04
#define DIRECTORY_LEAVE     0x05
#define QUERY               0x06
#define AGENT_JOIN          0x07
#define AGENT_LEAVE         0x08
#define DIRECTORY_UPDATE    0x09
#define DISCONNECT          0x0a
#define NEED_DIRECTORY      0x0b
#define UPDATE_EDGE         0x0c
#define UPDATE_EDGES        0x0d
#define SEND_UPDATES        0x0e
#define ACK_UPDATES         0x0f
#define START               0x10
#define SAVE                0x11
#define DUMP                0x12
#define READY_NV_NE         0x13
#define READY_NV_NE_INT     0x14
#define NV                  0x15
#define RV                  0x16
#define READY_SYNC          0x17
#define READY_SYNC_INT      0x18
#define SYNC                0x19
#define HAVE_UPDATE         0x1a
#define OUT_VN              0x1b
#define UPDATE              0x1c
#define RESET               0x1d
#define CHK_T               0x1e
#define VA                  0x1f
#ifdef CONFIG_CS
#define CS_UPDATE           0x20
#define CS_LB               0x21
#endif
#define SIMPLE_SYNC         0x22
#define SIMPLE_SYNC_DONE    0x23
#ifdef CONFIG_AUTOSCALE
#define AS_QUERY            0x24
#define AS_SCALE            0x25
#endif
#define HEARTBEAT           0xff

#define DO_ADD 0x40
#define DO_START    (START+DO_ADD)
#define DO_SAVE     (SAVE+DO_ADD)
#define DO_DUMP     (DUMP+DO_ADD)
#define DO_UPDATE   (UPDATE+DO_ADD)
#ifdef CONFIG_CS
#define DO_CS_LB    (CS_LB+DO_ADD)
#endif
#define DO_RESET    (RESET+DO_ADD)
#define DO_CHK_T    (CHK_T+DO_ADD)
#define DO_VA       (VA+DO_ADD)

/** Global per-process stop variable */
extern volatile sig_atomic_t global_shutdown;
extern localnum_t local_base;
extern localnum_t local_max;

#endif
