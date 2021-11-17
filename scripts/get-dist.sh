#!/bin/bash

source vars.sh

OUT=${OUT:-dist}

pdsh -R ssh -w $(cat $NODES) sh -c "\"ls -1 ${SCRATCH}/elga.agent.*log | xargs -n1 tail -$P \"" | sed 's/ gn.*$//' | sed 's/^.* nV=//' | sed 's/nE=//' | sed 's/,//g' > $OUT
