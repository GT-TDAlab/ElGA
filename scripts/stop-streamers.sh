#!/bin/bash

source vars.sh

pushd $(dirname $0)
thisdir=$(pwd)
popd

pdsh -w ^$NODES $thisdir/stop_streamer_int.sh
