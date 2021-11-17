#!/bin/bash -e

function P() {
    pdsh -w ^/cscratch/nodes-all "$@"
}
function D() {
    P "until sudo dnf $@; do sleep 1; done"
}
function I() {
    D install -y $@
}

I dnf-plugins-core
#D config-manager --set-enabled powertools
I numactl numactl-devel hwloc hwloc-devel
I datamash
#P "echo 153600 | sudo tee /proc/sys/vm/nr_hugepages"
#P "echo 0 | sudo tee /proc/sys/vm/nr_hugepages"
P mkdir -p /scratch/elga
