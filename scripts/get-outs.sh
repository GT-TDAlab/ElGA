#!/bin/bash

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <dest folder>"
    exit
fi

pushd $(dirname $0)
cdir=$(pwd)
popd
bname=$(basename $0)
me=$cdir/$bname

source $cdir/vars.sh

if [ "$#" -eq 2 ]; then
    # Cat into 4
    pushd ${SCRATCH}
    rm -f full.*.out
    ls -1 *.out | xargs -P8 -n4 sh -c 'cat "$0" "$@" > full.$$.$(hostname).out'
    popd

    cp ${SCRATCH}/full.*.out "$2/"
else
    mkdir -p "$1"
    pdsh -f 128 -w ^$NODES $me go "$1"
    pushd "$1"
fi
