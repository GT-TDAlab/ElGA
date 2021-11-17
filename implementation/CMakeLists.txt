# ----------------------------------------------------------------------------
# Author: Kasimir Gabert
#
# Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
# (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Please see the LICENSE.md file for license information.
# ----------------------------------------------------------------------------

# ----------------------------------------------------------------------------
# Project setup
cmake_minimum_required (VERSION 3.11)

project (ElGA VERSION 1.0 LANGUAGES CXX)

include (GNUInstallDirs)

set(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3")

# ------------------------------------------------------------------------
# Default to a Release build
if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()
endif()

# ----------------------------------------------------------------------------
# Set the dependencies

find_package(ZeroMQ 4.3 REQUIRED)

# Note: c++17 is not known to CMake for Intel 19
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

# Check if numa exists for OSX
find_library(NUMA_LIB numa)
if(NUMA_LIB)
    add_definitions(-DUSE_NUMA)
endif()

add_subdirectory(third-party/abseil-cpp)

# ----------------------------------------------------------------------------
# Compile-time options
set(ALG "WCC" CACHE STRING "The algorithm to run (BFS, PR, WCC, LPA, KCore)")
if (ALG STREQUAL "PR")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_PAGERANK")
elseif (ALG STREQUAL "WCC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_WCC")
elseif (ALG STREQUAL "KCore")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_KCORE")
elseif (ALG STREQUAL "BFS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_BFS -DCONFIG_START_VTX")
elseif (ALG STREQUAL "LPA")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_LPA")
else()
    message(FATAL_ERROR "Unknown algorithm")
endif()

option(SYM_BFS "Run BFS in an undirected manner" ON)
if(SYM_BFS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_SYM_BFS")
endif()

option(ARCH_NATIVE "Compile to use native features for release builds" ON)
if (ARCH_NATIVE AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

option(USE_CMS "Enable count-min sketching" ON)
if(USE_CMS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_CS")
endif()
option(SEND_MSGS_EARLY "Send messages early (potential optimization)" OFF)
set(SEND_MSGS_EARLY_LIMIT 1000 CACHE STRING "Limit for sending messages early")
if (SEND_MSGS_EARLY)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_SEND_MSG_EARLY -DSEND_MSG_EARLY_LIMIT=${SEND_MSGS_EARLY_LIMIT}")
endif()
option(DUMP_EL "Dump edge lists" ON)
if (DUMP_EL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDUMP_EL")
endif()
option(DUMP_BL "Dump Blogel files" OFF)
if (DUMP_BL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDUMP_BL")
endif()
option(DUMP_BL_SYM "Dump symmetric Blogel files" OFF)
if (DUMP_BL_SYM)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDUMP_BL_SYM")
endif()
option(DUMP_CS_STATE "Dump count-min sketch state" OFF)
if (DUMP_CS_STATE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDUMP_CS_STATE")
endif()
option(CONFIG_USE_AGENT_CACHE "Cache agent lookups")
if (CONFIG_USE_AGENT_CACHE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_USE_AGENT_CACHE")
endif()
option(CONFIG_MAINTAIN_LRU "Maintain an LRU set for participant connections")
if (CONFIG_MAINTAIN_LRU)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_MAINTAIN_LRU")
endif()
option(CONFIG_TIME_FIND_AGENTS "Output the time to find agents")
if (CONFIG_TIME_FIND_AGENTS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_TIME_FIND_AGENTS")
endif()
option(CONFIG_USE_CRC_HASH "Use CRC hashing")
if (CONFIG_USE_CRC_HASH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_USE_CRC")
endif()
option(CONFIG_USE_WANG_HASH "Use Wang hashing" ON)
if (CONFIG_USE_WANG_HASH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_USE_WANG")
endif()
option(CONFIG_USE_ABSEIL_HASH "Use abseil hashing")
if (CONFIG_USE_ABSEIL_HASH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_USE_ABSEIL")
endif()
option(CONFIG_USE_NUMA "Use internal NUMA assignments")
if (CONFIG_USE_NUMA)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_USE_NUMA")
endif()
option(DUMP_MSG_DIST "Dump the number of messages send")
if (DUMP_MSG_DIST)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDUMP_MSG_DIST")
endif()
option(CONFIG_GC "Perform garbage collection" ON)
if (CONFIG_GC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_GC")
endif()
option(CONFIG_PREPARE_SEND "Use prepare_send instead of allocating and copying")
if (CONFIG_PREPARE_SEND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_PREPARE_SEND")
endif()
option(CONFIG_TIME_INGESTION "Time the ingestion rate" ON)
if (CONFIG_TIME_INGESTION)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_TIME_INGESTION")
endif()
option(CONFIG_TACTIVATE "Use half activations for WCC")
if (CONFIG_WCC AND CONFIG_TACTIVATE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_TACTIVATE")
endif()
option(CONFIG_AUTOSCALE "Enable autoscaling")
if (CONFIG_AUTOSCALE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_AUTOSCALE")
endif()

set(START_PORT 17200 CACHE STRING "Config option START_PORT")
if (START_PORT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTART_PORT=${START_PORT}")
endif()
set(PUB_OFFSET 100 CACHE STRING "Config option PUB_OFFSET")
if (PUB_OFFSET)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPUB_OFFSET=${PUB_OFFSET}")
endif()
set(PULL_OFFSET 200 CACHE STRING "Config option PULL_OFFSET")
if (PULL_OFFSET)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPULL_OFFSET=${PULL_OFFSET}")
endif()
set(HIGHWATERMARK 500000 CACHE STRING "Config option HIGHWATERMARK")
if (HIGHWATERMARK)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHIGHWATERMARK=${HIGHWATERMARK}")
endif()
set(STARTING_VAGENTS 100 CACHE STRING "Config option STARTING_VAGENTS")
if (STARTING_VAGENTS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTARTING_VAGENTS=${STARTING_VAGENTS}")
endif()
set(DEF_EDGE_BATCH_SIZE 1073741824 CACHE STRING "Default size to allocate for batch edges")
if (DEF_EDGE_BATCH_SIZE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEF_EDGE_BATCH_SIZE=${DEF_EDGE_BATCH_SIZE}")
endif()
set(MID_BATCH_SIZE 100000000 CACHE STRING "Size to send before reading a full batch")
if (MID_BATCH_SIZE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMID_BATCH_SIZE=${MID_BATCH_SIZE}")
endif()
set(LRU_LIMIT 10240 CACHE STRING "LRU limit if used")
if (LRU_LIMIT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLRU_LIMIT=${LRU_LIMIT}")
endif()
set(PAGERANK_SUPERSTEPS 20 CACHE STRING "Number of supersteps to run PageRank for")
if (PAGERANK_SUPERSTEPS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPAGERANK_SUPERSTEPS=${PAGERANK_SUPERSTEPS}")
endif()

set(TABLE_WIDTH 262144 CACHE STRING "Width of sketch table")
if (TABLE_WIDTH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTABLE_WIDTH=${TABLE_WIDTH}")
endif()
set(TABLE_DEPTH 8 CACHE STRING "Depth of sketch table")
if (TABLE_DEPTH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTABLE_DEPTH=${TABLE_DEPTH}")
endif()
set(REP_THRESH 10000000000 CACHE STRING "Replication threshold")
if (REP_THRESH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_REP_THRESH=${REP_THRESH}")
endif()

set(AUTOSCALE_QUERY_RATE_THRESHOLD_HIGH 5000 CACHE STRING "High query rate threshold in qps for autoscaling")
if(AUTOSCALE_QUERY_RATE_THRESHOLD_HIGH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAUTOSCALE_QUERY_RATE_THRESHOLD_HIGH=${AUTOSCALE_QUERY_RATE_THRESHOLD_HIGH}")
endif()
set(AUTOSCALE_QUERY_RATE_THRESHOLD_LOW 10 CACHE STRING "Low query rate threshold in qps for autoscaling")
if(AUTOSCALE_QUERY_RATE_THRESHOLD_LOW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAUTOSCALE_QUERY_RATE_THRESHOLD_LOW=${AUTOSCALE_QUERY_RATE_THRESHOLD_LOW}")
endif()
set(AUTOSCALE_QR_TARGET 150 CACHE STRING "Target for queries per agent")
if(AUTOSCALE_QR_TARGET)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAUTOSCALE_QR_TARGET=${AUTOSCALE_QR_TARGET}")
endif()
set(AUTOSCALE_MAX_AGENTS 2048 CACHE STRING "Max agents for autoscaling")
if(AUTOSCALE_MAX_AGENTS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAUTOSCALE_MAX_AGENTS=${AUTOSCALE_MAX_AGENTS}")
endif()
set(AUTOSCALE_MIN_AGENTS 64 CACHE STRING "Min agents for autoscaling")
if(AUTOSCALE_MIN_AGENTS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAUTOSCALE_MIN_AGENTS=${AUTOSCALE_MIN_AGENTS}")
endif()
set(AUTOSCALE_EMA 15 CACHE STRING "Number of values to keep in the exponential moving average")
if (AUTOSCALE_EMA)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAUTOSCALE_EMA=${AUTOSCALE_EMA}")
endif()

set(CONFIG_SAVE_DIR /scratch/elga CACHE STRING "Local directory for agents to save to")
if (CONFIG_SAVE_DIR)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_SAVE_DIR=${CONFIG_SAVE_DIR}")
endif()

# ----------------------------------------------------------------------------
# Add targets and code

add_subdirectory (src)

# Compile the main executable
add_executable (ElGA src/main.cpp)

target_include_directories(ElGA PRIVATE
    ${ZeroMQ_INCLUDE_DIR})

# Check if numa exists
if(NOT NUMA_LIB OR NOT CONFIG_USE_NUMA)
    target_link_libraries (ElGA LINK_PRIVATE
        elgalib
        ${ZeroMQ_LIBRARY}
        pthread
    )
else()
    target_link_libraries (ElGA LINK_PRIVATE
        elgalib
        ${ZeroMQ_LIBRARY}
        numa
        hwloc
        pthread
    )
endif()

set_property(TARGET ElGA PROPERTY CXX_STANDARD 17)

install (TARGETS ElGA DESTINATION ${CMAKE_INSTALL_BINDIR})

# ----------------------------------------------------------------------------
# Add in tests
enable_testing ()

add_subdirectory (test)
