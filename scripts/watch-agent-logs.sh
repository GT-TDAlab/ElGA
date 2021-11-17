#!/bin/bash

LOG=${LOG:-agent.0.0}

watch -n 2 'tail -n $(($(tput lines)-4)) /scratch/elga/elga.'"$LOG"'.log | expand | cut -c1-$(($(tput cols)-4))'
