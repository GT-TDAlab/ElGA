#!/bin/bash

pushd $(dirname $0) 2>/dev/null
cdir=$(pwd)
bname=$(basename $0)
me=$cdir/$bname

source $cdir/vars.sh

if [ "$#" -eq 1 ]; then
    if [ $(ps faux | grep ElGA | grep -v grep | grep -v ssh | wc -l) -gt 2 ]; then
        echo SKIP
    else
        echo Running--$(hostname)
        export LD_PRELOAD=/cscratch/libmimalloc.so.1.7 && export MIMALLOC_PAGE_RESET=0;
        for ((nid=0; nid<NUMA; ++nid)); do
            for ((pni=0; pni<PN; ++pni)); do
                numactl -m $nid -N $nid -- $ELGA -d $dm_ip -B $(((nid*(PN+1)+pni)*P)) -P $P agent $(cat /ip) >& ${SCRATCH}/elga.$nid.$pni.log &
            done
        done
    fi
else
    pdsh -f 64 -w ^$NODES $me go
fi

popd 2>/dev/null
