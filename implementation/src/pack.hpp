/**
 * ElGA packing helpers
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "types.hpp"

#ifndef PACK_HPP
#define PACK_HPP

namespace elga {

    const size_t pack_msg_uint64_size = sizeof(msg_type_t)+sizeof(uint64_t);
    const size_t pack_msg_size_size = sizeof(msg_type_t)+sizeof(size_t);
    const size_t pack_msg_size_nv = sizeof(msg_type_t)+sizeof(size_t)*2;
    const size_t pack_msg_agent_size = sizeof(msg_type_t)+sizeof(uint64_t);
    const size_t pack_msg_batch_size = sizeof(msg_type_t)+sizeof(batch_t);
    const size_t pack_msg_update_size = sizeof(msg_type_t)+sizeof(update_t);
    const size_t pack_msg_unv_une_size = sizeof(msg_type_t)+sizeof(int64_t)+sizeof(double);

    template <typename T>
    inline __attribute__((always_inline))
    void pack_single(char *&msg, T t) {
        *(T*)msg = t;
        msg += sizeof(T);
    }

    template <typename T>
    inline __attribute__((always_inline))
    void unpack_single(const char *&msg, T &t) {
        t = *(T*)msg;
        msg += sizeof(T);
    }

    inline __attribute__((always_inline))
    void pack_msg_uint64(char *&msg, msg_type_t t, uint64_t i) {
        *(msg_type_t*)msg = t;
        msg += sizeof(msg_type_t);
        *(uint64_t*)msg = i;
        msg += sizeof(uint64_t);
    }

    inline __attribute__((always_inline))
    void pack_nm_msg_uint64(char *msg, msg_type_t t, uint64_t i) {
        char *ptr = msg;
        pack_msg_uint64(ptr, t, i);
    }

    inline __attribute__((always_inline))
    void pack_msg(char *&msg, msg_type_t t) {
        pack_single(msg, t);
    }

    inline __attribute__((always_inline))
    void pack_size(char *&msg, size_t s) {
        pack_single(msg, s);
    }

    inline __attribute__((always_inline))
    void pack_nv(char *&msg, size_t nv, size_t ne) {
        pack_size(msg, nv);
        pack_size(msg, ne);
    }

    inline __attribute__((always_inline))
    msg_type_t unpack_msg(const char *&msg) {
        msg_type_t t;
        unpack_single(msg, t);
        return t;
    }

    inline __attribute__((always_inline))
    uint64_t pack_agent(uint64_t agent_ser, aid_t aid) {
        return agent_ser | (uint64_t)aid << 49;
    }

    inline __attribute__((always_inline))
    void unpack_agent(uint64_t in, uint64_t &agent_ser, aid_t &aid) {
        agent_ser = in & ((1llu << 49)-1);
        aid = in >> 49;
    }

    inline __attribute__((always_inline))
    void pack_msg_agent(char *&msg, msg_type_t t, uint64_t agent_ser, aid_t aid) {
        *(msg_type_t*)msg = t;
        msg += sizeof(msg_type_t);
        uint64_t i = pack_agent(agent_ser, aid);
        *(uint64_t*)msg = i;
        msg += sizeof(uint64_t);
    }

    inline __attribute__((always_inline))
    void unpack_msg_agent(char *&msg, msg_type_t &t, uint64_t &agent_ser, aid_t &aid) {
        t = *(msg_type_t*)msg;
        msg += sizeof(msg_type_t);

        uint64_t i = *(uint64_t*)msg;
        unpack_agent(i, agent_ser, aid);

        msg += sizeof(uint64_t);
    }

    inline __attribute__((always_inline))
    void pack_msg_update(char *&msg, msg_type_t t, update_t u) {
        *(msg_type_t*)msg = t;
        msg += sizeof(msg_type_t);
        *(update_t*)msg = u;
        msg += sizeof(update_t);
    }

    inline __attribute__((always_inline))
    void unpack_update(const char *&msg, update_t &u) {
        u = *(update_t*)msg;
        msg += sizeof(update_t);
    }

    inline __attribute__((always_inline))
    void pack_msg_batch(char *&msg, msg_type_t t, batch_t b) {
        pack_msg(msg, t);
        pack_single(msg, b);
    }

    inline __attribute__((always_inline))
    void unpack_batch(const char *&msg, batch_t &b) {
        unpack_single(msg, b);
    }

    inline __attribute__((always_inline))
    void pack_msg_unv_une(char *&msg, msg_type_t t, double unv, int64_t une) {
        pack_msg(msg, t);
        pack_single(msg, unv);
        pack_single(msg, une);
    }

    inline __attribute__((always_inline))
    void unpack_unv_une(const char *&msg, double &unv, int64_t &une) {
        unpack_single(msg, unv);
        unpack_single(msg, une);
    }

}

#endif
