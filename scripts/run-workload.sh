#!/bin/bash

source vars.sh

pdsh -f 64 -w ^$NODES bash -c "\"$PRE for cid in {1..20}; do $ELGA -d $dm_ip client workload >& ${SCRATCH}/elga.client.\\\$cid.log & done;\""
