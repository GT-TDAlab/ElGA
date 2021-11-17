#!/bin/bash -e

trap die INT
function die() {
    echo "KILLED"
    exit 1
}

pushd $(dirname $0)

for x in /cscratch/email/run/*; do
    ST=$(grep -c TIME /scratch/elga/elga.agent.0.0.log)
    echo $x $ST
    echo ==============
    ./elga.sh streamer +el +no+batch $x;
    until [ $(grep -c TIME /scratch/elga/elga.agent.0.0.log) -gt $ST ]; do echo -n .; sleep 1; done;
    echo DONE
done

popd
