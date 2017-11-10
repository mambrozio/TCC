#!/bin/bash

executable=$1
repeat=$2
compiler=$3
directory=$4
name=$5
#DATE=`date +%Y-%m-%d:%H:%M:%S`
#mkdir $directory/$DATE
#echo $repeat
#echo $directory
#echo $directory/perf-$name-$n

n=0
for filename in ./examples/*.byte; do
    if [[ "${filename}" != @("./examples/fizzbuzz.byte"|"./examples/minimize-int.byte"|"./examples/minimize-real.byte"|"./examples/odd_even.byte"|"./examples/relop.byte"|"./examples/sum-of-f.byte") ]]; then
        n=$(($n + 1))
        echo 'Running' $executable $filename'...'
        perf stat -o $compiler/$directory/perf-$name-$n -ecpu-cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses -r $repeat $executable $filename > /dev/null
    fi
done
