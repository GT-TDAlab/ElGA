/**
 * ElGA Main entry point
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

#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <thread>
#include <locale>

#include "chatterbox.hpp"
#include "address.hpp"

#include "directory_master.hpp"
#include "directory.hpp"
#include "streamer.hpp"
#include "client.hpp"
#include "agent.hpp"

#ifdef USE_NUMA
#ifdef CONFIG_USE_NUMA
#include <numa.h>
#include <hwloc.h>
#endif
#endif

/** Helper function for printing the program usage line to cout */
int usage_() {
    std::cout << "Usage: ElGA [-h] [-v] [-P num] -d directory-ip command "
        "[help] [command args...]" << std::endl;
    return 0;
}

/** Helper function for printing out a help message */
int help_() {
    std::cout << "\n"
        "Main interface to ElGA, an elastic graph processing system.\n"
        "This executable contains the various components of ElGA and\n"
        "handles passing execution off to ElGA's components.\n\n"
        "Arguments:\n"
        "    command : the ElGA system to run\n"
        "    command args : arguments passed to the subcommand\n"
        "    help : provide help on running subcommand arguments\n\n"
        "ElGA systems:\n"
        "    directory-master : this runs once per cluster and its IP\n"
        "        is given to all others with -d <ip>\n"
        "    directory : runs directory servers managing elastic agents\n"
        "    streamer : streams changes into ElGA\n"
        "    client : queries ElGA\n"
        "    agent : runs agents on the node to maintain the graph and\n"
        "        execute algorithms\n\n"
        "Options:\n"
        "    -d : required, IP address of the directory master, required\n"
        "    -B : local number base to start at for multiple processes\n"
        "    -P : limit the processors to this number for agents\n"
        "    -h : display this help message\n"
        "    -v : display version information\n\n"
        "To get help from a subcommand, use the keyword 'help'\n"
        ;
    return EXIT_SUCCESS;
}

#ifdef CONFIG_USE_NUMA
void thread(localnum_t ln, localnum_t num_cores, hwloc_topology_t &topology,
        std::string command, const elga::ZMQAddress &dm,
        int argc, const char **argv) {
#else
void thread(localnum_t ln, localnum_t num_cores,
        std::string command, const elga::ZMQAddress &dm,
        int argc, const char **argv) {
#endif
    int res = 0;

    #ifdef CONFIG_USE_NUMA
    // Pin the CPU and set the numa affinity
    if (num_cores > 1) {
        hwloc_obj_t core = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, ln);
        if (core != NULL) {
            hwloc_cpuset_t set = hwloc_bitmap_dup(core->cpuset);
            hwloc_bitmap_singlify(set);
            hwloc_set_cpubind(topology, set, HWLOC_CPUBIND_THREAD);
            hwloc_bitmap_free(set);
        } else {
            std::cerr << "[ElGA : Main] WARNING: Not enough physical cores. Not pinning." << std::endl;
        }
        numa_set_preferred(numa_node_of_cpu(sched_getcpu()));
        std::cerr << "[ElGA : Main] Pinned cores." << std::endl;
    }
    #endif

    // Pass forward the command
    if (command == "streamer")
        res = elga::streamer::main(argc, argv, dm, ln);
    else if (command == "directory-master")
        res = elga::directory_master::main(argc, argv, dm, ln);
    else if (command == "client")
        res = elga::client::main(argc, argv, dm, ln);
    else if (command == "directory")
        res = elga::directory::main(argc, argv, dm, ln);
    else if (command == "agent")
        res = elga::agent::main(argc, argv, dm, ln);
    else
        throw arg_error("Unknown command.");

    if (res != 0)
        throw std::runtime_error("Non-zero thread exit");
}

/** Keep track of whether networking has been setup for exceptions */
bool networking_setup_;

/** The main entry point */
int main_(int argc, char **argv) {
    // Parse arguments
    char c = '\0';
    std::string command;
    std::string dir_ip;

    opterr = 0;

    #ifdef CONFIG_USE_NUMA
    // Discover the hwlock topology
    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    // Find the number of cores
    size_t num_cores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
    #else
    size_t num_cores = std::thread::hardware_concurrency();
    #endif
    size_t ln_base = 0;

    bool custom_num_cores = false;

    while (optind < argc) {
        if ((c = getopt(argc, argv, "B:P:d:hv?")) != -1) {
            // Handle the case arguments
            switch (c) {
                case '?': { usage_(); return -1; }
                case 'h': { usage_(); return help_(); }
                case 'd': {
                              dir_ip.assign(std::string(optarg));
                              break;
                          }
                case 'B': {
                              ln_base = std::stoul(std::string(optarg));
                              if (ln_base >= std::numeric_limits<localnum_t>::max())
                                  throw arg_error("Base too large");
                              break;
                          }
                case 'P': {
                              num_cores = std::stoul(std::string(optarg));
                              custom_num_cores = true;
                              break;
                          }
                default: { throw arg_error("Unknown argument."); }
            }
        } else {
            // Treat the remainder as the command followed by positional
            // arguments
            // Find the command given, ensure it is correct, and pass it
            // to the appropriate command main

            if (argv[optind] == NULL)
                throw arg_error("No command given.");
            // Register the command
            command.assign(argv[optind]);
            break;
        }
    }

    if (dir_ip.length() == 0)
        throw arg_error("directory-ip is a required argument");

    // Use multiple threads only if appropriate
    if (!custom_num_cores && command != "agent" && command != "directory")
        num_cores = 1;

    // Setup the necessary networking
    elga::ZMQChatterbox::Setup(num_cores);
    networking_setup_ = true;

    // Handle the directory information
    elga::ZMQAddress directory(dir_ip, 0);

    const char **nargv = (const char**)&(argv[optind]);
    int nargc = argc-optind;

    local_base = ln_base;
    local_max = ln_base+num_cores;
    if (local_max < local_base)
        throw arg_error("Num cores, base too large");

    std::vector<std::thread> ln_threads;
    for (localnum_t ln = local_base; ln < local_max; ++ln) {
        #ifdef CONFIG_USE_NUMA
        ln_threads.push_back(std::thread([ln, num_cores, &topology, &command, &directory, nargc, &nargv]{ thread(ln, num_cores, topology, command, directory, nargc, nargv); }));
        #else
        ln_threads.push_back(std::thread([ln, num_cores, &command, &directory, nargc, &nargv]{ thread(ln, num_cores, command, directory, nargc, nargv); }));
        #endif
    }

    for (auto& t : ln_threads)
        t.join();

    return EXIT_SUCCESS;
}

void si_handle(int s);

void set_handler() {
    struct sigaction si_handler;
    si_handler.sa_handler = si_handle;
    sigemptyset(&si_handler.sa_mask);
    si_handler.sa_flags = SA_RESTART;
    sigaction(SIGINT, &si_handler, NULL);
}

void si_handle(int s) {
    std::cerr << "Ctrl+C Caught. Shutting down..." << std::endl;
    // Begin a graceful shutdown
    global_shutdown = 1;
}

/** A wrapper that captures and displays argument errors */
int main(int argc, char **argv) {
    networking_setup_ = false;
    int ret = -1;

    // Address file limits
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
        throw std::runtime_error("Unable to get file limits");
    limit.rlim_cur = limit.rlim_max;
    if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
        throw std::runtime_error("Unable to set file limits");

    #ifdef DEBUG_VERBOSE
    std::cerr << "Set file limit to " << limit.rlim_cur << std::endl;
    #endif

    // Register to capture Ctrl+C
    set_handler();

    // Use regional/default locale
    std::locale def_locale("");
    std::cout.imbue(def_locale);
    std::cerr.imbue(def_locale);

    try {
        ret = main_(argc, argv);
    } catch(arg_error &e) {
        std::cerr << "Argument Error: " << e.what() << std::endl;
        usage_();
        if (networking_setup_) {
            elga::ZMQChatterbox::Teardown();
            networking_setup_ = false;
        }
        return EXIT_FAILURE;
    }
    if (networking_setup_) {
        elga::ZMQChatterbox::Teardown();
        networking_setup_ = false;
    }

    return ret;
}
