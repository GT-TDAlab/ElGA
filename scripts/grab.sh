#!/bin/bash -e

source vars.sh

NODES=${NODES:-/cscratch/nodes}

function P() {
    pdsh -w ^$NODES "$@"
}

P 'mkdir ~/elga/experiments/in/$(hostname -s)'
P 'cp /scratch/elga/*{log,cs.dump} ~/elga/experiments/in/$(hostname -s)/'
