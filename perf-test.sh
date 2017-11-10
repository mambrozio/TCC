#!/bin/bash

n=0
for filename in ./examples/*.byte; do
    if [[ "${filename}" != @("./examples/fizzbuzz.byte"|"./examples/minimize-int.byte"|"./examples/minimize-real.byte"|"./examples/odd_even.byte"|"./examples/relop.byte"|"./examples/sum-of-f.byte") ]]; then
        n=$(($n + 1))
        perf stat -o results/perf-hybrid-$n -ecpu-cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses -r 15 ./hybrid $filename
    fi
done

n=0
for filename in ./examples/*.byte; do
    if [[ "${filename}" != @("./examples/fizzbuzz.byte"|"./examples/minimize-int.byte"|"./examples/minimize-real.byte"|"./examples/odd_even.byte"|"./examples/relop.byte"|"./examples/sum-of-f.byte") ]]; then
        n=$(($n + 1))
        perf stat -o results/perf-c-minilua-$n -ecpu-cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses -r 15 ./c-minilua $filename
    fi
done
