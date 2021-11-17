#!/bin/bash

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <dump type (el, bl)> <dest in hdfs>"
    exit
fi

pushd $(dirname $0) 2>/dev/null
cdir=$(pwd)
popd 2>/dev/null
bname=$(basename $0)
me=$cdir/$bname

source $cdir/vars.sh

H=~/hadoop/hadoop-3.2.2/bin/hdfs

if [ "$#" -eq 3 ]; then
    UPL=8
    for x in ${SCRATCH}/*.${2}.dump; do
        $H dfs -Ddfs.replication=1 -put "$x" "$3/" &
        ((--UPL))
        if [ $UPL -eq 0 ]; then
            wait
            UPL=8
        fi
    done
    wait
else
    $H dfs -mkdir -p "$2/"
    pdsh -f 64 -w ^$NODES $me go "$1" "$2"
fi
