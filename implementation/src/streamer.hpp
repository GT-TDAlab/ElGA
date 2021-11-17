/**
 * ElGA streamer
 *
 * This command is responsible for running a streamer, which will either
 * proxy incoming network data or read from an edge list and forward
 * appropriately into ElGA.
 *
 * Authors: Kaan Sancak, Kasimir Gabert, Yusuf Ozkaya
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <tuple>

#include "participant.hpp"

namespace elga {

    class Streamer;

    namespace streamer {

        const size_t BUF_SIZE = 4096;

        /** Main entry point for the streamer command */
        int main(int argc, const char **argv, const ZMQAddress &directory_master, localnum_t ln);

        /** Parse a given edge and return tuple */
        std::tuple<edge_t, bool> parse_edge(std::fstream &instream, bool el=false);
    }

    /**
     * The streamer is responsible for streaming edges from an input into
     * the graph. It can be viewed as a proxy to ElGA.
     */
    class Streamer : public Participant {
        private:
            absl::flat_hash_map<uint64_t, std::vector<edge_t>> changes_;
            size_t batch_size_;
            bool batch_;
            bool wait_;
            size_t mb_;
        public:
            /** Initialize the streamer pointed at the given dm */
            Streamer(const ZMQAddress &directory_master) :
                Participant(ZMQAddress(), directory_master, false),
                batch_size_(0), batch_(true), wait_(false),
                batch_count(0), mb_(0)
                { }

            /** Set the mini-batch size */
            void set_mb(size_t val) { mb_ = val; }

            /** Add or delete a new edge */
            void change_edge(edge_t e, bool insert);

            /** Parse the given file and stream results */
            void parse_file(std::string fname, bool el);

            /** Generate a simple random graph from rank r/P */
            void rg(uint64_t N, uint64_t M, uint32_t r, uint32_t P);

            /** Listen for incoming edges at the given ZeroMQ address string */
            void listen(std::string listen_addr);

            /** Read batches from a buffer, returns number of edges parsed */
            size_t parse_incoming_batch(const uint64_t* data, uint64_t count);

            /** Wait until the directory is received and ready to use */
            void wait_until_ready();

            /** Send the full batch */
            void send_batch();

            /** Set whether to batch edges or not */
            void set_batch(bool val) { batch_ = val; }

            /** Setup for waiting for batch results */
            void wait_batch() { sub(SYNC); wait_ = true; }

            /** Handle additional messages */
            bool handle_msg(zmq_socket_t sock, msg_type_t t, const char *data, size_t size);

            /** Keep track of the seen batch counts */
            size_t batch_count;
    };

}
