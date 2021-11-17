#!/bin/bash

source $(dirname $0)/vars.sh

((--PN))
((--NUMA))

if ! ip addr | grep "$DIR_IP"/$mask; then
    echo 'Setting up dir ip'
    sudo ip addr add dev $nic "$DIR_IP/$mask"
fi

if ! ip addr | grep "$dm_ip"/$mask; then
    echo 'Setting up dm ip'
    sudo ip addr add dev $nic "$dm_ip/$mask"
fi

echo "Have dm_ip=$dm_ip"

# Start the DM and dir
echo "Starting DM and D..."
$ELGA -d $dm_ip directory-master &>${SCRATCH}/elga.dm.log &
#for x in {1..16}; do
for x in {1..1}; do
    $ELGA -d $dm_ip -B $x -P 1 directory $DIR_IP &>${SCRATCH}/elga.d.$x.log &
done

echo "Starting agents..."
pdsh -f 64 -w ^$NODES bash -c "\"$PRE for nid in {0..$NUMA}; do for pni in {0..$PN}; do numactl -m \\\$nid -N \\\$nid -- $ELGA -d $dm_ip -B \\\$(((nid*(${PN}+1)+pni)*${P})) -P $P agent \$(cat /ip) >& ${SCRATCH}/elga.agent.\\\$nid.\\\$pni.log & done; done;\""
