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
    ls -1 *.el.dump | grep -v full | xargs -P8 -n4 sh -c 'cat "$0" "$@" > full.$$.$(hostname).el.dump'
    popd

    cp ${SCRATCH}/full.*.el.dump "$2/"
else
    mkdir -p "$1"
    pdsh -f 64 -w ^$NODES $me go "$1"
    pushd "$1"

    echo -n waiting
    until ls -1 *.el.dump | wc -l | grep $((64*8)); do
        echo -n .
        sleep 1
    done
    echo

    y=0
    for x in *.el.dump; do
        mv $x $y
        ((++y))
    done
    popd
fi
