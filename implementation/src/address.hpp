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

#ifndef ADDRESS_HPP
#define ADDRESS_HPP

#include "types.hpp"

#include <string>

#include <zmq.h>

namespace elga {

    typedef enum addr_type {
        REQUEST,
        PUBLISH,
        PULL
    } addr_type_t;

    /** Keep track of ZMQ addresses (local and remote) */
    class ZMQAddress {
        private:
            /** Pre-compute and store the remote addr */
            std::string remote_addr_;
            /** Pre-compute and store the local addr */
            std::string local_addr_;

            /** Pre-compute and store the remote pub addr */
            std::string remote_pub_addr_;
            /** Pre-compute and store the local pub addr */
            std::string local_pub_addr_;

            /** Pre-compute and store the remote pull addr */
            std::string remote_pull_addr_;
            /** Pre-compute and store the local pull addr */
            std::string local_pull_addr_;

            /** Store the IP address and local number */
            uint32_t addr_;
            localnum_t localnum_;

            /** Pre-compute the strings for efficiency */
            void precompute_strings_();
        public:
            /** Create the address based on a string, e.g. command line */
            ZMQAddress(std::string addr, localnum_t localnum);
            /** Create an address given the IP and localnum serialized */
            ZMQAddress(uint64_t ser_addr);
            /** Create an empty address */
            ZMQAddress() : remote_addr_(), local_addr_(), remote_pub_addr_(),
                    local_pub_addr_(),
                    remote_pull_addr_(), local_pull_addr_(),
                    addr_(0), localnum_(0) { }

            /** Support swapping */
            void swap(ZMQAddress &that) {
                std::swap(remote_addr_, that.remote_addr_);
                std::swap(local_addr_, that.local_addr_);
                std::swap(remote_pub_addr_, that.remote_pub_addr_);
                std::swap(local_pub_addr_, that.local_pub_addr_);
                std::swap(remote_pull_addr_, that.remote_pull_addr_);
                std::swap(local_pull_addr_, that.local_pull_addr_);
                std::swap(addr_, that.addr_);
                std::swap(localnum_, that.localnum_);
            }

            /** Retrieve the best connection string */
            const char *get_conn_str(const ZMQAddress &myself, addr_type_t at) const;
            /** Retrieve the remote connection string */
            const char *get_remote_str() const { return remote_addr_.c_str(); }
            /** Retrieve the local connection string */
            const char *get_local_str() const { return local_addr_.c_str(); }
            /** Retrieve the remote pub connection string */
            const char *get_remote_pub_str() const { return remote_pub_addr_.c_str(); }
            /** Retrieve the local pub connection string */
            const char *get_local_pub_str() const { return local_pub_addr_.c_str(); }
            /** Retrieve the remote pull connection string */
            const char *get_remote_pull_str() const { return remote_pull_addr_.c_str(); }
            /** Retrieve the local pull connection string */
            const char *get_local_pull_str() const { return local_pull_addr_.c_str(); }

            /** Return the address */
            uint32_t get_addr() const { return addr_; }
            /** Return the localnum */
            localnum_t get_localnum() const { return localnum_; }

            /** Return the serialized address */
            uint64_t serialize() const;

            /** Return whether the address is an empty/zero address */
            bool is_zero() const;
    };

}

#endif
