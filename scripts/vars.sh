#!/bin/bash

P=${P:-1}
PN=${PN:-8}
NUMA=${NUMA:-4}
dm_ip=$(cat /ip)
DIR_IP=$(cat /ip)
mask=16
nic=ens785
NODES=${NODES:-/cscratch/nodes}
NODES_LONG=${NODES_LONG:-/cscratch/nodes-long}
STREAMP=${STREAMP:-8}

ELGA=/cscratch/ElGA
ELGA=~/elga/public-staging/implementation/build/ElGA

PRE="export LD_PRELOAD=/usr/lib64/libvma.so.9.2.2:/cscratch/libmimalloc.so.2.0 && export MIMALLOC_PAGE_RESET=0 && export MIMALLOC_RESERVE_HUGE_OS_PAGES=4800 && export VMA_SPEC=latency && export VMA_RX_POLL=-1;"
PRE="export LD_PRELOAD=/cscratch/libmimalloc.so.2.0 && export MIMALLOC_PAGE_RESET=0 && export MIMALLOC_RESERVE_HUGE_OS_PAGES=4800;"

SCRATCH=/scratch/elga/
