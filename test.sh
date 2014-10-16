#!/bin/bash

# Alexandre Lucchesi Alencar
# 
# Programação Paralela - Prof. George Teodoro
# 
# Exercício de Programação 03 - Sum Tree

# sh test.sh 25 5
max_numbers=$1
max_runs=$2

file1_prefix="./res/sum_"
file2_prefix="./res/time_"
file3_prefix="./res/numbers_"

if [ $# -ne 2 ]; then
    echo "Usage: sh test.sh <2^max_numbers> <max_runs>"
    exit 1
fi

# Cleans data files...
data_files=$(ls ./res)
if [ ${#data_files} -gt 0 ]; then
    rm ./res/*
fi

# Generates input files...
for num_order in `seq 20 $max_numbers` # Starting at 2^20 until 2^{max_numbers}.
do
    # Output file storing randomly generated numbers.
#    suffix=$((num_order-2))
    data_file=$file3_prefix$num_order".dat"

    num_count=$(echo "2^$num_order" | bc)
    ./sumtree -gen $data_file $num_count
done

# Performs benchmark...
procs=(1 2 4 8 16)
for t in ${procs[@]} # Varies number of processes...
do
    file1=$file1_prefix$t".csv"
    file2=$file2_prefix$t".csv"

    # Writes headers.
    for i in `seq 1 $max_runs` # Execution numbers...
    do
        # There's an empty (first) column...
        printf "\t, $i" >> $file1
        printf "\t, $i" >> $file2
    done
    printf "\n" >> $file1
    printf "\n" >> $file2

    for num_order in `seq 20 $max_numbers` # Starting at 2^20 until 2^{max_numbers}.
    do
        printf "2^$num_order" >> $file1
        printf "2^$num_order" >> $file2

        # File storing the generated numbers to be used in the current iteration.
#        suffix=$((num_order-2))
        data_file=$file3_prefix$num_order".dat"

        for j in `seq 1 $max_runs` # Runs the program <max_runs> times...
        do
            # Runs the program with a fixed timeout...
            output=$(mpiexec -n $t sumtree < $data_file)
            res=($output) # Convert to array (splitting at ' ')...
            sum=${res[0]}
            time_ms=${res[1]}
            printf ", $sum" >> $file1
            printf ", $time_ms" >> $file2
        done
        printf "\n" >> $file1
        printf "\n" >> $file2
    done
done

