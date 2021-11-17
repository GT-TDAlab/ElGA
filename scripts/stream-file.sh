#!/bin/bash

source $(dirname $0)/vars.sh

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <folder> [full-dne]"
    exit
fi

nl="$NODES_LONG"
f=$1

EL="+el"
if [ "$#" -eq 2 ]; then
    EL=""
    echo "-el"
fi

num_f=$(ls -1 $f | wc -l)
num_nl=$(wc -l $nl | cut -f1 -d' ')
((num_per_node=num_f/num_nl))
echo "Have $num_per_node per node"
echo "Have $num_nl nodes"
echo "Have $num_f files"

cdir=$(pwd)

echo "Starting streamers..."
fctr=0
for node in $(cat $nl); do
    ssh -q $node bash -c "cd $cdir && for ((i=$fctr;i<$fctr+$num_per_node;++i)); do $ELGA -d $dm_ip streamer $EL $f/\$i >${SCRATCH}/elga.streamfile.\$i.log 2>&1 & done; wait" &
    ((fctr+=num_per_node))
done
if [ "$fctr" -ne "$num_f" ]; then
    echo "Entering overflow..."
    num_per_node=1
    for node in $(cat $nl); do
        ssh -q $node bash -c "cd $cdir && for ((i=$fctr;i<$fctr+$num_per_node;++i)); do $ELGA -d $dm_ip streamer $EL $f/\$i >${SCRATCH}/elga.streamfile.\$i.log 2>&1 & done; wait" &
        ((fctr+=num_per_node))
        if [ "$fctr" -eq "$num_f" ]; then
            break
        fi
    done
    echo "done"
fi
