/**
 * ElGA chatterbox and networking
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include <iostream>

#include <chrono>
#include <thread>

#include "chatterbox.hpp"

using namespace elga;

/** ZMQ specific context used globally per-process */
void *G_zmq_context_;

void ZMQChatterbox::Setup(int num_threads) {
    G_zmq_context_ = zmq_ctx_new();
    if (zmq_ctx_set(G_zmq_context_, ZMQ_IO_THREADS, num_threads) != 0)
        throw std::runtime_error("Unable to create zmq threads");
    if (zmq_ctx_set(G_zmq_context_, ZMQ_MAX_SOCKETS, num_threads*1024000) != 0)
        throw std::runtime_error("Unable to set zmq socket limit");
}

void ZMQChatterbox::Teardown() {
    zmq_ctx_destroy(G_zmq_context_);
}

zmq_socket_t elga::socket_(int type, uint64_t affinity, bool use_buffering) {
    zmq_socket_t socket = zmq_socket(G_zmq_context_, type);
    if (socket == NULL) { perror("socket_"); throw std::runtime_error("Unable to create socket"); }

    /*
    int sock_linger = 1000;
    if (zmq_setsockopt(socket, ZMQ_LINGER, &sock_linger, sizeof(sock_linger)) != 0)
        throw std::runtime_error("Unable to set socket to linger");
        */

    if (affinity > 0) {
        if (zmq_setsockopt(socket, ZMQ_AFFINITY, &affinity, sizeof(affinity)) != 0)
            throw std::runtime_error("Unable to set socket affinity");
    }

    int backlog = (1<<15);
    if (zmq_setsockopt(socket, ZMQ_BACKLOG, &backlog, sizeof(backlog)) != 0)
        throw std::runtime_error("Unable to set message backlog limit");
    int hwm = (use_buffering) ? HIGHWATERMARK : 1;
    hwm = 0;
    if (zmq_setsockopt(socket, ZMQ_SNDHWM, &hwm, sizeof(hwm)) != 0)
        throw std::runtime_error("Unable to set high water mark for sending");
    if (zmq_setsockopt(socket, ZMQ_RCVHWM, &hwm, sizeof(hwm)) != 0)
        throw std::runtime_error("Unable to set high water mark for receiving");

    /*
    int buf = (1<<12);
    if (zmq_setsockopt(socket, ZMQ_SNDBUF, &buf, sizeof(buf)) != 0)
        throw std::runtime_error("Unable to set buffer for sending");
    if (zmq_setsockopt(socket, ZMQ_RCVBUF, &buf, sizeof(buf)) != 0)
        throw std::runtime_error("Unable to set buffer for receiving");
    */

    return socket;
}

void elga::bind_(zmq_socket_t socket, const char *addr) {
    int bind_res = zmq_bind(socket, addr);
    if (bind_res != 0) throw std::runtime_error("Unable to bind");
}

/** Helper function to connect to a remote address */
void connect_(zmq_socket_t socket, const ZMQAddress remote_addr, const ZMQAddress &my_addr, addr_type_t at) {
    int conn_res = zmq_connect(socket, remote_addr.get_conn_str(my_addr, at));
    if (conn_res != 0) throw std::runtime_error("Unable to connect");
}

/** Helper function to disconnect from a remote address */
void disconnect_(zmq_socket_t socket, const ZMQAddress remote_addr, const ZMQAddress &my_addr, addr_type_t at) {
    int conn_res = zmq_disconnect(socket, remote_addr.get_conn_str(my_addr, at));
    if (conn_res != 0) {
        perror("disconnecting");
        if (errno == EINTR) {
            return;
        }
        throw std::runtime_error("Unable to disconnect");
    }
}

ZMQChatterbox::ZMQChatterbox(const ZMQAddress &addr) :
        sock_rep_(NULL), sock_pub_(NULL), sock_sub_(NULL), sock_pull_(NULL), addr_(addr), last_heartbeat_() {

    // Create our sockets
    sock_rep_ = socket_(ZMQ_REP);
    sock_pub_ = socket_(ZMQ_PUB);
    sock_sub_ = socket_(ZMQ_SUB);
    sock_pull_ = socket_(ZMQ_PULL);

    // Open and connect all of our sockets
    if (!addr_.is_zero()) {
        bind_(sock_rep_, addr_.get_local_str());
        bind_(sock_rep_, addr_.get_remote_str());

        bind_(sock_pub_, addr_.get_local_pub_str());
        bind_(sock_pub_, addr_.get_remote_pub_str());

        bind_(sock_pull_, addr_.get_local_pull_str());
        bind_(sock_pull_, addr_.get_remote_pull_str());
    }
}

ZMQChatterbox::~ZMQChatterbox() {
    // Close all of our open sockets
    if (sock_rep_) zmq_close(sock_rep_);
    if (sock_pub_) zmq_close(sock_pub_);
    if (sock_sub_) zmq_close(sock_sub_);
    if (sock_pull_) zmq_close(sock_pull_);
}

std::vector<zmq_socket_t> ZMQChatterbox::poll(long timeout) {
    // Build the structure to poll with
    zmq_pollitem_t polls[3];
    polls[0] = {sock_rep_, 0, ZMQ_POLLIN, 0};
    polls[1] = {sock_sub_, 0, ZMQ_POLLIN, 0};
    polls[2] = {sock_pull_, 0, ZMQ_POLLIN, 0};

    // Find and return any received messages
    std::vector<zmq_socket_t> res;

    // Now, actually poll (blocking)
    int ret = zmq_poll(polls, 3, timeout);
    if (ret < 0) {
        // Check if EINTR is the issue; that is okay to continue
        if (errno == EINTR)
            return res;
        throw std::runtime_error("Unable to poll");
    }

    if (polls[0].revents & ZMQ_POLLIN)
        res.push_back(sock_rep_);
    if (polls[1].revents & ZMQ_POLLIN)
        res.push_back(sock_sub_);
    if (polls[2].revents & ZMQ_POLLIN)
        res.push_back(sock_pull_);

    return res;
}

void ZMQChatterbox::send(zmq_socket_t sock, const char *data, size_t size, bool nowait) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQChatterbox] sending : " << sock << " , " << size << std::endl;
    #endif
    //int ret = zmq_send(sock, data, size, (nowait)?ZMQ_DONTWAIT:0);
    int ret = 0;
    do {
        if (ret == -1)
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        ret = zmq_send(sock, data, size, (nowait)?0:0);
    } while (ret == -1 && errno == EAGAIN);
    if (ret < 0) { perror("ZMQChatterbox::send"); throw std::runtime_error("Unable to send"); }
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQChatterbox] sent : " << sock << " , " << size << std::endl;
    #endif
}

void ZMQChatterbox::pub(const char* data, size_t size) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQChatterbox] publishing : " << size << std::endl;
    #endif

    send(sock_pub_, data, size);

    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQChatterbox] published : " << size << std::endl;
    #endif
}

ZMQMessage ZMQChatterbox::prepare_pub(size_t size) {
    return ZMQMessage(sock_pub_, size);
}

void ZMQChatterbox::sub_connect(const ZMQAddress &addr) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQChatterbox] connecting to publisher : " << addr.get_conn_str(addr_, PUBLISH) << std::endl;
    #endif

    connect_(sock_sub_, addr, addr_, PUBLISH);
}

void ZMQChatterbox::sub_disconnect(const ZMQAddress &addr) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQChatterbox] disconnecting from publisher : " << addr.get_conn_str(addr_, PUBLISH) << std::endl;
    #endif

    disconnect_(sock_sub_, addr, addr_, PUBLISH);
}

void ZMQChatterbox::sub(msg_type_t type) {
    sub((const char*)&type, sizeof(type));
}

void ZMQChatterbox::sub(const char *data, size_t size) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQChatterbox] subscribing var data : " << size << std::endl;
    #endif

    if (zmq_setsockopt(sock_sub_, ZMQ_SUBSCRIBE, data, size) != 0)
        throw std::runtime_error("Unable to subscribe");
}

void ZMQChatterbox::unsub(const char *data, size_t size) {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQChatterbox] subscribing var data : " << size << std::endl;
    #endif

    if (zmq_setsockopt(sock_sub_, ZMQ_UNSUBSCRIBE, data, size) != 0)
        throw std::runtime_error("Unable to unsubscribe");
}

void ZMQChatterbox::ack(zmq_socket_t sock) {
    // An ack is a zero-sized message
    send(sock, NULL, 0);
}

bool ZMQChatterbox::heartbeat(bool send) {
    if (last_heartbeat_.distance_us() < heartbeat_us)
        return false;
    if (!send) return true;
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQChatterbox] sending heartbeat" << std::endl;
    #endif

    // Publish a heartbeat message
    msg_type_t hb = HEARTBEAT;
    pub((char*)&hb, sizeof(hb));

    last_heartbeat_ = timer::TimePoint();
    return true;
}

ZMQRequester::ZMQRequester(const ZMQAddress server, const ZMQAddress &myself, addr_type_t at, bool use_buffering) :
            server_(server), sock_(NULL) {
    // Build a new connection to the given server
    int conn = (at == PULL) ? ZMQ_PUSH : ZMQ_REQ;
    sock_ = socket_(conn, 0, use_buffering);
    connect_(sock_, server_, myself, at);
}

ZMQRequester::ZMQRequester() : server_(), sock_(NULL) { }

ZMQRequester::~ZMQRequester() {
    if (sock_ != NULL)
        int ret = zmq_close(sock_);
    sock_ = NULL;
}

void ZMQRequester::send(const char *data, size_t size, bool nowait) {
    ZMQChatterbox::send(sock_, data, size, nowait);
}

void ZMQRequester::send(msg_type_t type) {
    ZMQChatterbox::send(sock_, (const char*)&type, sizeof(type));
}

ZMQMessage ZMQRequester::prepare_send(size_t size) {
    ZMQMessage msg(sock_, size);

    return msg;
}

ZMQMessage ZMQRequester::read() {
    return ZMQMessage(sock_);
}

void ZMQRequester::wait_ack() {
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQRequester] waiting for ack" << std::endl;
    #endif
    int size = zmq_recv(sock_, NULL, 0, 0);
    if (size != 0) throw std::runtime_error("Failed to receive ack");
    #ifdef DEBUG_VERBOSE
    std::cerr << "[ElGA : ZMQRequester] ack received" << std::endl;
    #endif
}

ZMQMessage::ZMQMessage(zmq_socket_t sock, bool block) : msg_part_(), sock_(sock), size_(0), closed_(false) {
    // Initialize the message
    if (zmq_msg_init(&msg_part_) != 0) throw std::runtime_error("Unable to init msg");

    // Read out the message
    size_ = zmq_msg_recv(&msg_part_, sock_, (block) ? 0 : ZMQ_DONTWAIT);

    if (size_ == -1 && !block && errno == EAGAIN) {
        // We are not blocking, and there are no messages
        zmq_msg_close(&msg_part_);
        closed_ = true;
        return;
    }

    if (size_ < 0) throw std::runtime_error("Unable to read message");

    // The message is now held up in the internal zmq_msg
    data_ = (char*)zmq_msg_data(&msg_part_);
}

ZMQMessage::ZMQMessage(zmq_socket_t sock, size_t size) : msg_part_(), sock_(sock), size_(0), closed_(false) {
    // Initialize and allocate the message
    if (zmq_msg_init_size(&msg_part_, size) != 0) throw std::runtime_error("Unable to init alloc msg");

    // Set the size
    size_ = (int)size;

    // The output buffer is now held up in the internal zmq_msg
    data_ = (char*)zmq_msg_data(&msg_part_);
}

ZMQMessage::~ZMQMessage() {
    if (closed_) return;
    // Close and free the zmq_msg
    zmq_msg_close(&msg_part_);
}

ZMQMessage::ZMQMessage(ZMQMessage&& that) {
    std::swap(msg_part_, that.msg_part_);
    std::swap(sock_, that.sock_);
    std::swap(size_, that.size_);
    std::swap(data_, that.data_);
    that.closed_ = true;
}

ZMQMessage& ZMQMessage::operator=(ZMQMessage&& that) {
    if (this != &that) {
        std::swap(msg_part_, that.msg_part_);
        std::swap(sock_, that.sock_);
        std::swap(size_, that.size_);
        std::swap(data_, that.data_);
        that.closed_ = true;
    }
    return *this;
}

void ZMQMessage::send() {
    int ret_size = zmq_msg_send(&msg_part_, sock_, 0);
    if (ret_size != size_) throw std::runtime_error("Error while sending");
}
