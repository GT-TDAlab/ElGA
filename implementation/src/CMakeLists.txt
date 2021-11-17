# ----------------------------------------------------------------------------
# Author: Kasimir Gabert
#
# Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
# (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Please see the LICENSE.md file for license information.
# ----------------------------------------------------------------------------

set (elgalib_src
    agent.cpp
    agentfull.cpp
    agentbsp.cpp
    agentlbsp.cpp
    types.cpp
    address.cpp
    client.cpp
    participant.cpp
    directory.cpp
    directory_master.cpp
    chatterbox.cpp
    streamer.cpp
    timer.cpp
    integer_hash.cpp
    countsketch.cpp
    countminsketch.cpp
    consistenthasher.cpp
    pralgorithm.cpp
    wccalgorithm.cpp
    kcorealgorithm.cpp
    bfsalgorithm.cpp
    lpaalgorithm.cpp
    )

add_library (elgalib STATIC ${elgalib_src})

set_property(TARGET elgalib PROPERTY CXX_STANDARD 17)

target_include_directories(elgalib PUBLIC
    ${PROJECT_SOURCE_DIR}/third-party/abseil-cpp/
    ${ZeroMQ_INCLUDE_DIR})

target_link_libraries (elgalib LINK_PRIVATE
    ${ZeroMQ_LIBRARY}
    absl::hash
    absl::flat_hash_map
    absl::flat_hash_set
    )
