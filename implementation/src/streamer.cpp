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

#include "streamer.hpp"
#include "pack.hpp"
#include "timer.hpp"

#include <fstream>

#include <thread>
#include <chrono>

#include <random>
#include <unordered_set>

using namespace elga;

namespace elga::streamer {

    void print_usage() {
        std::cout << "Usage: streamer [options] edge-list [edge-list ...]" << std::endl;
    }

    int print_help() {
        std::cout << "\n"
            "Interface to ElGA streamer.\n"
            "Streamer reads the given file and parses the edgelist\n"
            "Handles the parsed edges.\n"
            "Options:\n"
            "    help : display this help message\n" <<
            "    rg N M r P : stream a random graph\n" <<
            "      with N vertices, M edges, P nodes\n" <<
            "      from rank r\n" <<
            "    listen addr : listen on the given address" <<
            std::endl;
        return 0;
    }

    int main(int argc, const char **argv, const ZMQAddress &directory_master, localnum_t ln) {
        if (argc <= 1) { print_usage(); return 0; }
        // Do an initial pass over all arguments checking for 'help'
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "help") {
                print_usage();
                return print_help();
            }
        }

        if (ln != 0) throw std::runtime_error("Unimplemented.");

        // Create the streamer
        Streamer s(directory_master);

        s.wait_until_ready();

        bool el = false;

        // Do a second pass, processing each file
        for (int i = 1; i < argc; ++i) {
            std::string fname(argv[i]);

            timer::Timer t(fname);

            t.tick();

            if (fname == "rg") {
                if (argc-i < 5)
                    throw std::runtime_error("Expecting arguments");

                uint64_t N = std::stoull(std::string(argv[i+1]));
                uint64_t M = std::stoull(std::string(argv[i+2]));
                uint32_t r = std::stoul(std::string(argv[i+3]));
                uint32_t P = std::stoul(std::string(argv[i+4]));

                i += 4;

                std::cerr << "[ElGA : Streamer] Random graph: " << N << " " << M << " from " << r << "/" << P << std::endl;

                s.rg(N, M, r, P);
            } else if (fname == "+el") {
                el = true;
            } else if (fname == "+no+el") {
                el = false;
            } else if (fname == "+batch") {
                s.set_batch(true);
            } else if (fname == "+no+batch") {
                s.set_batch(false);
            } else if (fname == "+wait+batch") {
                s.wait_batch();
            } else if (fname == "+mb") {
                if (argc-i < 2)
                    throw std::runtime_error("Expecting arguments");

                s.set_mb(std::stoull(std::string(argv[++i])));
            } else if (fname == "listen") {
                if (argc-i < 2)
                    throw std::runtime_error("Expecting arguments");

                std::string listen_addr(argv[++i]);

                std::cerr << "[ElGA : Streamer] Listening: " << listen_addr << std::endl;
                s.listen(listen_addr);
            } else {
                // Setup processing for the given file
                s.parse_file(fname, el);
            }

            t.tock();

            std::cerr << "[ElGA : Streamer] " << t << std::endl;
        }
        std::cerr << "[ElGA : Streamer] end" << std::endl;

        return 0;
    }

    std::tuple<edge_t, bool> parse_edge(std::fstream &instream, bool el) {
        std::tuple<edge_t, bool> res;
        int insert_delete = 0;
        double ignore_weight;
        uint64_t ignore_timestamp;
        if (el) {
            instream >> std::get<0>(res).src >> std::get<0>(res).dst;
            insert_delete = 1;
        } else {
            instream >> insert_delete >> std::get<0>(res).src >> std::get<0>(res).dst >> ignore_weight >> ignore_timestamp;
        }
        if (instream.fail())
            throw std::runtime_error("Invalid parameter while parsing edge.");
        if (insert_delete != 1 && insert_delete != -1) {
            throw std::runtime_error("Insert/delete flag must be first entry in line");
        }
        std::get<1>(res) = insert_delete > 0;
        return res;
    }
}

void Streamer::wait_until_ready() {
    // Wait until the streamer is ready
    while (!ready_ && do_poll()) {
        if (global_shutdown) {
            std::cerr << "[ElGA : Streamer] shutting down" << std::endl;
            return;
        }
    }
}

size_t Streamer::parse_incoming_batch(const uint64_t* data, uint64_t count) {
    size_t nE = 0;
    size_t ctr = 0;

    for (uint64_t idx = 0; idx < count;) {
        if (global_shutdown) {
            std::cerr << "[ElGA : Streamer] shutting down" << std::endl;
            return false;
        }

        if (idx + 1 == count) {
            if (data[idx] == 0) return nE;
            throw std::runtime_error("Unknown protocol issue");
        }

        // Parse the edge
        edge_t e;
        e.src = data[idx++];
        e.dst = data[idx++];

        if (batch_) {
            bool dummy;
            uint64_t agent = find_agent(e, IN, true, 0, dummy);
            changes_[agent].push_back(e);
            ++batch_size_;
        } else {
            change_edge(e, true);
        }

        nE++;
        ctr++;
        if (ctr >= 1000000) {
            std::cerr << "[ElGA : Streamer] in-batch sent nE=" << nE << std::endl;
            ctr = 0;
        }
    }

    return nE;
}

void Streamer::parse_file(std::string fname, bool el) {
    std::fstream reader;
    char buf[streamer::BUF_SIZE];
    reader.rdbuf()->pubsetbuf(buf, sizeof(buf));
    reader.open(fname);

    size_t nE = 0;
    size_t ctr = 0;
    auto cur_batch_count = batch_count;
    while (!reader.eof() && reader.good()) {
        if (global_shutdown) {
            std::cerr << "[ElGA : Streamer] shutting down" << std::endl;
            return;
        }
        // FIXME occasionally check for updates
        if (reader.peek() == '\r' || reader.peek() == '\n' || reader.peek() == -1) {
            reader.get();
        } else if (reader.peek() == '%' || reader.peek() == '#') {
            reader.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else {
            // Parse the edge
            auto change = streamer::parse_edge(reader, el);
            if (reader.peek() != '\r' && reader.peek() != '\n' && reader.peek() != -1)
                throw std::runtime_error("Extra data on input line");
            if (batch_) {
                auto & [e, ins] = change;
                if (!ins)
                    change_edge(e, ins);
                else {
                    bool dummy;
                    uint64_t agent = find_agent(e, IN, true, 0, dummy);
                    changes_[agent].push_back(e);
                    ++batch_size_;
                }
            } else {
                change_edge(std::get<0>(change), std::get<1>(change));
            }

            nE++;
            ctr++;
            if (ctr >= 10000000) {
                std::cerr << "[ElGA : Streamer] sent nE=" << nE << std::endl;
                ctr = 0;
            }

            if (mb_ > 0 && ctr % mb_ == 0) {
                if (wait_) {
                    std::cerr << "[ElGA : Streamer] waiting" << std::endl;
                    while (batch_count <= cur_batch_count && do_poll()) {
                        if (global_shutdown) {
                            std::cerr << "[ElGA : Streamer] shutting down" << std::endl;
                            return;
                        }
                    }
                    #ifdef CONFIG_WAIT_MB
                    if ((ctr/mb_) % 15 == 0)
                        do_poll();
                    #endif
                    cur_batch_count = batch_count;
                }
            }
        }
    }
    if (batch_) {
        timer::Timer send_timer {"batch_send"};
        send_timer.tick();
        // Finish sending the batch
        send_batch();
        send_timer.tock();
        std::cerr << "[ElGA : Streamer] " << send_timer << " sent batch" << std::endl;
    }
    if (wait_ && (mb_ == 0 || ctr % mb_ != 0)) {
        std::cerr << "[ElGA : Streamer] waiting" << std::endl;
        while (batch_count <= cur_batch_count && do_poll()) {
            if (global_shutdown) {
                std::cerr << "[ElGA : Streamer] shutting down" << std::endl;
                return;
            }
        }
        do_poll();
    }
}

void Streamer::rg(uint64_t N, uint64_t M, uint32_t r, uint32_t P) {
    uint64_t my_E = M/P;
    if (r == P-1) my_E += M%P;

    uint64_t my_N_start = r*(N/P);
    uint64_t my_N_end =  ((r == P-1) ? N : (r+1)*(N/P))-1;

    std::unordered_set<edge_t> chosen;
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<vertex_t> src_gen(my_N_start, my_N_end);
    std::uniform_int_distribution<vertex_t> dst_gen(0, N-1);

    std::cerr << "[ElGA : Streamer] generating " << my_E << " edges from vertices " << my_N_start << "-" << my_N_end << std::endl;

    for (uint64_t cur_E = 0; cur_E < my_E; ++cur_E) {
        // If we have already chosen it find another
        edge_t e;
        size_t ctr = 0;
        do {
            if (ctr++ > 100)
                throw std::runtime_error("Use a better random graph model");

            e.src = src_gen(mt);
            e.dst = dst_gen(mt);
            if (e.src == e.dst) continue;
        } while (chosen.find(e) != chosen.end());

        chosen.insert(e);

        // Add the edge
        change_edge(e, true);

        if (cur_E % 1000000 == 0)
            std::cerr << "[ElGA : Streamer] " << cur_E << std::endl;
    }
}

void Streamer::listen(std::string listen_addr) {
    zmq_socket_t receiver = socket_(ZMQ_PULL, 0, false);
    try {
        bind_(receiver, listen_addr.c_str());

        size_t nE = 0;
        size_t ctr = 0;

        long poll_time = 20000;

        timer::Timer batch_timer {"batch"};

        while (!global_shutdown) {
            // Two loops let us measure when the queue is empty, to generate times
            zmq_pollitem_t polls[1];
            polls[0] = {receiver, 0, ZMQ_POLLIN, 0};

            int ret = zmq_poll(polls, 1, poll_time);
            if (!(polls[0].revents & ZMQ_POLLIN) || (ret < 0 && errno == EINTR)) {
                if (ctr > 0) {
                    // This means: we were processing, now we have nothing
                    // this is the end of a 'batch'
                    batch_timer.tock();
                    std::cerr << "[ElGA : Streamer] " << batch_timer << " batch size: " << ctr << " total: " << nE << std::endl;

                    if (batch_) {
                        timer::Timer send_timer {"batch_send"};
                        send_timer.tick();
                        // Finish sending the batch
                        send_batch();
                        send_timer.tock();
                        std::cerr << "[ElGA : Streamer] " << send_timer << " sent batch" << std::endl;
                    }

                    ctr = 0;
                    // Now, set a longer timeout
                    poll_time = 2500;
                }
                continue;
            }
            if (ret < 0)
                throw std::runtime_error("Unable to poll");

            if (ctr == 0) {
                batch_timer.tick();
                poll_time = 100;
            }

            while (!global_shutdown) {

                // Garbage collect on the receiver
                size_t fd_size = sizeof(uint32_t);
                uint32_t fd;
                zmq_getsockopt(receiver, ZMQ_EVENTS, &fd, &fd_size);

                ZMQMessage msg { receiver, false };

                size_t msg_size = msg.size();
                if (msg_size == -1)
                    break;

                // Parse all edges in the message
                size_t msg_count = msg.size()/sizeof(uint64_t);

                size_t newly_added = parse_incoming_batch((uint64_t*)msg.data(), msg_count);

                nE += newly_added;
                ctr += newly_added;

                if (ctr % 1000000 == 0) {
                    std::cerr << "[ElGA : Streamer] sent nE=" << nE << std::endl;
                }

                if (batch_) {
                    if (batch_size_ >= MID_BATCH_SIZE) {
                        timer::Timer send_timer {"mid_batch_send"};
                        send_timer.tick();
                        // Finish sending the batch
                        send_batch();
                        send_timer.tock();
                        batch_size_ = 0;
                        std::cerr << "[ElGA : Streamer] " << send_timer << " sent batch" << std::endl;
                    }
                }
            }
        }
        std::cerr << "[ElGA : Streamer] " << " total: " << nE << std::endl;
    } catch(...) {
        zmq_close(receiver);
        throw;
    }
    zmq_close(receiver);
}

void Streamer::change_edge(edge_t e, bool insert) {
    // We want to send both an IN and OUT edge

    // Find the agents to send the edges to
    bool dummy;

    uint64_t agent_in_ser = find_agent(e, IN, true, 0, dummy);

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Streamer] in : " << e.dst << "->" << agent_in_ser << " || " << ZMQAddress(agent_in_ser).get_remote_str() << std::endl;
    #endif

    // Check whether the agent requesters are cached; if not, get them
    ZMQRequester &agent_in_req = get_requester(agent_in_ser);

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Streamer] sending edge " << e.src << "->" << e.dst << " to " << agent_in_ser << std::endl;
    #endif

    // Pack and send the appropriate edges
    char data[pack_msg_update_size];
    char *data_ptr = data;
    update_t u;
    u.e = e;
    u.et = IN;
    u.insert = insert;
    pack_msg_update(data_ptr, UPDATE_EDGE, u);
    agent_in_req.send(data, sizeof(data));
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : Streamer] in: " << &agent_in_req << std::endl;
    #endif
}

void Streamer::send_batch() {
    // Send them in bulk
    for (auto & [ag, el] : changes_) {
        ZMQRequester &agent_in_req = get_requester(ag);
        size_t msg_size = sizeof(msg_type_t) + el.size()*sizeof(update_t);
        char *msg = new char[msg_size];

        char *msg_ptr = msg;
        pack_msg(msg_ptr, UPDATE_EDGES);
        for (const edge_t & e : el) {
            update_t u;
            u.e = e;
            u.et = IN;
            u.insert = true;
            pack_single(msg_ptr, u);
        }

        agent_in_req.send(msg, msg_size);

        delete [] msg;
        el.clear();
    }
    changes_.clear();
}

bool Streamer::handle_msg(zmq_socket_t sock, msg_type_t t, const char *data, size_t size) {
    if (!wait_) return true;
    switch (t) {
        case SYNC: {
            size_t global_num_active = *(size_t*)data;
            if (global_num_active == 0) { ++batch_count; }
            break;
        }
    }
    return true;
}
