# ----------------------------------------------------------------------------
# Author: Kasimir Gabert
#
# Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
# (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Please see the LICENSE.md file for license information.
# ----------------------------------------------------------------------------

set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/tests)
set (CTEST_BINARY_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

include_directories("${PROJECT_SOURCE_DIR}/src")

file (GLOB test_files "test_*.cpp")

foreach (test_src ${test_files})
    string (REGEX REPLACE "(^.*/|\\.cpp$)" "" ex_name ${test_src})
    add_executable (${ex_name} ${test_src})

    target_link_libraries(${ex_name}
        elgalib
        ${ZeroMQ_LIBRARY}
        pthread
        )
    set_property(TARGET ${ex_name} PROPERTY CXX_STANDARD 17)

    add_test (NAME ${ex_name} COMMAND
        sh -c "
        ${CTEST_BINARY_DIRECTORY}/${ex_name}
        exit $?
        " --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(${ex_name} PROPERTIES TIMEOUT 120)
endforeach()

add_test (NAME BaseArgs COMMAND
        bash -c "
        ${PROJECT_BINARY_DIR}/ElGA 2>&1 | grep 'ip is a required' || exit 1
        ${PROJECT_BINARY_DIR}/ElGA -d 1.2.3.4 invalid-arg && exit 1
        ${PROJECT_BINARY_DIR}/ElGA -h 2>&1 | grep Options: || exit 1
        ${PROJECT_BINARY_DIR}/ElGA -? 2>&1 | grep Usage: || exit 1
        ${PROJECT_BINARY_DIR}/ElGA -? 2>&1 | grep -v Options: || exit 1
        ${PROJECT_BINARY_DIR}/ElGA -d bad 2>&1 | grep 'Unable to parse IP' || exit 1
        exit 0
        " --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test (NAME StreamerHelp COMMAND
        bash -c "
        ${PROJECT_BINARY_DIR}/ElGA -d 0 streamer help 2>&1 | grep 'Interface to ElGA streamer' || exit 1
        ${PROJECT_BINARY_DIR}/ElGA -d 0 streamer 2>&1 | grep 'Usage:' || exit 1
        exit 0
        " --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_test (NAME DirectoryHelp COMMAND
        bash -c "
        ${PROJECT_BINARY_DIR}/ElGA -d 0 directory help 2>&1 | grep 'The Directory service' || exit 1
        ${PROJECT_BINARY_DIR}/ElGA -d 0 directory 2>&1 | grep 'Usage:' || exit 1
        exit 0
        " --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test (NAME TestEdgeParsing COMMAND
        bash -c "
        timeout -k 30s 30s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.1 directory-master &
        sleep 1
        timeout -k 29s 29s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.1 -P 1 directory 127.0.3.1 &
        sleep 1
        timeout -k 28s 28s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.1 -P 1 agent 127.0.4.1 &
        sleep 1

        ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.1 streamer streams/correct_format 2>&1 || exit 1
        ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.1 streamer streams/invalid_characters 2>&1 | grep 'Invalid parameter' || exit 1
        ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.1 streamer streams/invalid_number_of_arguments 2>&1 | grep 'Extra data' || exit 1

        ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.1 client shutdown

        wait
        exit 0
        " --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test (NAME EmptyDirList COMMAND
        bash -c "
        ret=0
        ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 directory-master &
        ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 client directories | wc -l | grep '0$' || ret=1
        kill %1
        exit $ret
        " --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test (NAME TestShutdown COMMAND
        bash -c "
        ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 client shutdown &
        timeout -k 5s 5s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 directory-master
        ret=$?
        wait %1
        exit $ret
        " --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test (NAME SmallDirsAndShutdown COMMAND
        bash -c "
        ret=0
        timeout -k 30s 30s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 directory-master &

        timeout -k 30s 30s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 -P 1 directory 127.0.2.5 &
        timeout -k 30s 30s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 -P 1 directory 127.0.2.6 &
        timeout -k 30s 30s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 -P 1 directory 127.0.2.7 &

        sleep 1

        ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 client directories | sort -n | md5sum | grep 56a9f1195c71399da378afeece67e2af || ret=1
        ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 client shutdown
        wait
        exit $ret
        " --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test (NAME SmallDirsAndCShutdown COMMAND
        bash -c "
        ret=0
        timeout -k11s -s SIGINT 9s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 directory-master &

        timeout -k11s 11s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 -P 1 directory 127.0.2.5 &
        timeout -k11s 11s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 -P 1 directory 127.0.2.6 &
        timeout -k11s -s SIGINT 3s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 -P 1 directory 127.0.2.7 &

        sleep 1

        timeout -k9s 9s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 client directories | sort -n | md5sum | grep 56a9f1195c71399da378afeece67e2af || ret=1

        echo 'waiting for directory server to leave'

        sleep 1
        sleep 1
        sleep 1
        sleep 1
        sleep 1
        sleep 1

        echo 'making client call'

        timeout -k7s 7s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.3 client directories | sort -n | md5sum | grep b0bc70e663480859ea79507c7ce73938 || ret=1

        wait
        exit $ret
        " --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(SmallDirsAndCShutdown PROPERTIES TIMEOUT 13)

add_executable (_TestCompressedVAgents _test_compressed_vagents.cpp)

target_link_libraries(_TestCompressedVAgents
    elgalib
)
set_property(TARGET _TestCompressedVAgents PROPERTY CXX_STANDARD 17)
add_test (NAME TestCompressedVAgents COMMAND
        bash -c "
        timeout -k 30s 30s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.1 directory-master &
        sleep 1
        timeout -k 29s 29s ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.1 -P 1 directory 127.0.3.1 &
        sleep 1
        timeout -k 28s 28s ${PROJECT_BINARY_DIR}/ElGA -P 5 -d 127.0.2.1 agent 127.0.4.1 &
        sleep 1

        ${CTEST_BINARY_DIRECTORY}/_TestCompressedVAgents
        ret=$?

        ${PROJECT_BINARY_DIR}/ElGA -d 127.0.2.1 client shutdown

        wait
        exit $ret
        " --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

