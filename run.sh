#!/bin/sh

if [ $# -ne 2 ]; then
        echo "Usage: sh run.sh <nproc> <input>"
        exit 1
fi

nproc=$1
input=$2

mpiexec -n $1 sumtree < $input

