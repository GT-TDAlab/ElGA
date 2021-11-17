#!/bin/bash

NODES=${NODES:-/cscratch/nodes-all}

killall -9 ElGA &>/dev/null

pdsh -R ssh -w $(cat $NODES) sh -c '"killall ElGA; killall -9 ElGA; killall ElGA; killall -9 ElGA; rm -f /tmp/*{ovn,out,dump} /tmp/elga*log /tmp/feed* /tmp/dist*txt; rm -rf /scratch/elga/*"' &>/dev/null
