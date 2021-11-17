#!/bin/bash

source vars.sh

echo "Found dm_ip=$dm_ip"

echo "Starting streamers..."
pdsh -f 64 -R ssh -w $(cat $NODES) bash -c "\"$PRE for x in {00..$((STREAMP-1))}; do $ELGA -d $dm_ip streamer listen ipc:///tmp/feed\\\$x >& ${SCRATCH}/elga.stream\\\$x.log & done;\""
