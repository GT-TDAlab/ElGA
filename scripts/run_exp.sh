#!/bin/bash -e

FS_PREFIX=/cscratch/
NODES_PREFIX=${FS_PREFIX}nodes
NODES_FULL=${FS_PREFIX}nodes-full
EXP_DIR=${HOME}/elga/experiments
SCRATCH=/scratch/elga/

pushd $(dirname $0)

trap die INT
function die() {
    echo "KILLED"
    exit 1
}

ncheck=$(cat ${NODES_PREFIX}1)

EL=yes

START=${START:-0}
cnum=0

#for G in twitter-2010 friendster uk-2007-05 euall-5000 skitter-200 LiveJournal-100 amazon0601-2000 g30 gowalla-10000 patents-1000 pokec-1000; do
for G in datagen-9_3-zf datagen-9_4-fb; do
    for trial in {1..5}; do
    for NUMA in 4; do
    for NODES in 64; do
        for PN in 32; do
            if [ $START -gt $((++cnum)) ]; then continue; fi
            echo $cnum :: $G $NODES $PN
            echo ======================================================================
            echo -n cleanup
            ./cleanup.sh
            echo
            NUMA=$NUMA PN=$((PN/NUMA)) P=1 NODES=${NODES_PREFIX}$NODES ./start-elga.sh 2>/dev/null
            echo -n waiting for agents to start
            until tail -n 1 ${SCRATCH}/elga.d.1.log | sed 's/,//g' | grep $((NODES*PN)); do
                echo -n .
                sleep 5
            done
            echo

            echo -n waiting for heartbeats
            ssh $ncheck "bash -c \"until [ \\\$(grep -c HRTBEAT ${SCRATCH}/elga.agent.0.0.log) -gt 2 ]; do echo -n .; sleep 5; done\"" 2>/dev/null
            echo

            echo streaming file
            ./stream-file.sh $NODES_FULL ${FS_PREFIX}${G} $EL 2>/dev/null
            echo

            ssh $ncheck "bash -c \"until [ \\\$(ls ${SCRATCH}/elga.streamfile* 2>/dev/null | wc -l ) -gt 0 ]; do echo -n .; sleep 1; done\"" 2>/dev/null
            echo -n '>'

            until [ $(ps faux | grep stream | grep ssh | wc -l) -eq 0 ]; do
                echo -n .
                sleep 1
            done
            echo -n '>'

            until [ $(pdsh -w ^$NODES_PREFIX 'for x in ${SCRATCH}/elga.streamfile.*log; do tail -n 1 $x | grep -v end 2>/dev/null || true; done' 2>/dev/null | wc -l) -eq 0 ]; do
                echo -n .
                sleep 5
            done
            echo

            echo -n waiting for heartbeats
            ssh $ncheck "bash -c \"ST=\\\$(grep -c HRTBEAT ${SCRATCH}/elga.agent.0.0.log); until [ \\\$(grep -c HRTBEAT ${SCRATCH}/elga.agent.0.0.log) -gt \\\$((ST+3)) ]; do echo -n .; sleep 5; done\"" 2>/dev/null
            echo

            for b in {1..1}; do
                echo -n starting batch
                ./elga.sh client start
                echo

                echo -n waiting for heartbeats
                ssh $ncheck "bash -c \"ST=\\\$(grep -c HRTBEAT ${SCRATCH}/elga.agent.0.0.log); until [ \\\$(grep -c HRTBEAT ${SCRATCH}/elga.agent.0.0.log) -gt \\\$((ST+3)) ]; do echo -n .; sleep 5; done\"" 2>/dev/null
                echo

                echo -n waiting for batch
                ssh $ncheck "bash -c \"until [ \\\$(tail -n 5 ${SCRATCH}/elga.agent.0.0.log | grep -c 'B TIME') -gt 0 ]; do echo -n .; sleep 5; done\"" 2>/dev/null
                echo
            done

            echo -n shutdown and collect data
            ./elga.sh client shutdown
            sleep 2
            ./grab.sh 2>/dev/null
            THIS_DIR=${EXP_DIR}/scale/$G/$trial/$NODES':'$PN
            rm -rf ${THIS_DIR}
            mkdir -p ${THIS_DIR}
            mv ${EXP_DIR}/in/* ${THIS_DIR}
            echo
            echo DONE

            #echo ---
            #echo DUMP
            #./elga.sh client dump
            #echo -n waiting for heartbeats
            #ssh $ncheck "bash -c \"ST=\\\$(grep -c HRTBEAT ${SCRATCH}/elga.agent.0.0.log); until [ \\\$(grep -c HRTBEAT ${SCRATCH}/elga.agent.0.0.log) -gt \\\$((ST+3)) ]; do echo -n .; sleep 5; done\"" 2>/dev/null
            #echo
            #./upload-dumps.sh bl /un/$G
            #echo REALLY DONE

        done
    done
    done
    done
done

popd
