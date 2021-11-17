/**
 * ElGA chatterbox addresses
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "address.hpp"

#include <sstream>

#include <arpa/inet.h>

using namespace elga;

ZMQAddress::ZMQAddress(std::string addr, localnum_t localnum) {
    struct in_addr in_addr;

    // Extract the IP from the given string
    // We want to make sure we're using a 32 bit address representation
    static_assert(sizeof(addr_) == sizeof(struct in_addr));
    if (inet_aton(addr.c_str(), &in_addr) == 0)
        throw std::runtime_error("Unable to parse IP address");

    addr_ = in_addr.s_addr;
    localnum_ = localnum;

    // Now, pre-compute the strings
    precompute_strings_();
}

uint64_t ZMQAddress::serialize() const {
    // Pack the localnum and addr as localnum||addr
    return (((uint64_t)localnum_)<<32) | (uint64_t)addr_;
}

ZMQAddress::ZMQAddress(uint64_t ser_addr) {
    // Unpack the localnum and addr
    addr_ = (uint32_t)ser_addr;
    localnum_ = (localnum_t)(ser_addr >> 32);

    // Now, pre-compute the strings
    precompute_strings_();
}

const char * ZMQAddress::get_conn_str(const ZMQAddress &myself, addr_type_t at) const {
    // Return either local or remote, depending on whether the addresses
    // are the same
    if (addr_ == myself.get_addr() &&
            (localnum_ >= local_base &&
            localnum_ < local_max)) {
        if (at == PUBLISH)
            return get_local_pub_str();
        else if (at == REQUEST)
            return get_local_str();
        else if (at == PULL)
            return get_local_pull_str();
    } else {
        if (at == PUBLISH)
            return get_remote_pub_str();
        else if (at == REQUEST)
            return get_remote_str();
        else if (at == PULL)
            return get_remote_pull_str();
    }
    throw std::runtime_error("Unknown address type");
}

void ZMQAddress::precompute_strings_() {
    struct in_addr in_addr;

    in_addr.s_addr = addr_;

    // Now, re-convert to a string, and use that to build the remote
    // string for consistency
    std::string ip(inet_ntoa(in_addr));

    uint16_t port = localnum_ + START_PORT;

    std::ostringstream remote_addr_os;
    remote_addr_os << "tcp://" << ip << ":" << port;
    remote_addr_ = remote_addr_os.str();

    std::ostringstream local_addr_os;
    local_addr_os << "inproc://" << localnum_;
    local_addr_ = local_addr_os.str();

    std::ostringstream remote_pub_addr_os;
    remote_pub_addr_os << "tcp://" << ip << ":" << (port+PUB_OFFSET);
    remote_pub_addr_ = remote_pub_addr_os.str();

    std::ostringstream local_pub_addr_os;
    local_pub_addr_os << "inproc://" << (localnum_+PUB_OFFSET);
    local_pub_addr_ = local_pub_addr_os.str();

    std::ostringstream remote_pull_addr_os;
    remote_pull_addr_os << "tcp://" << ip << ":" << (port+PULL_OFFSET);
    remote_pull_addr_ = remote_pull_addr_os.str();

    std::ostringstream local_pull_addr_os;
    local_pull_addr_os << "inproc://" << (localnum_+PULL_OFFSET);
    local_pull_addr_ = local_pull_addr_os.str();
}

bool ZMQAddress::is_zero() const {
    if (addr_ == 0) return true;
    return false;
}
