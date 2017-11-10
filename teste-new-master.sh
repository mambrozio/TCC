#!/bin/bash

rm -rf clang/
rm -rf gcc
rm -rf cpu.info
rm -rf all-results
rm -rf all-results-csv
rm -rf all-results-xls

mkdir -p gcc/results-master-flagO2
mkdir -p gcc/results-prefetch-flagO2
mkdir -p gcc/results-c-minilua-flagO2
mkdir -p gcc/results-aggregate-flagO2

mkdir -p gcc/results-master-flagO3
mkdir -p gcc/results-prefetch-flagO3
mkdir -p gcc/results-c-minilua-flagO3
mkdir -p gcc/results-aggregate-flagO3

mkdir -p clang/results-master-flagO2
mkdir -p clang/results-prefetch-flagO2
mkdir -p clang/results-c-minilua-flagO2
mkdir -p clang/results-aggregate-flagO2

mkdir -p clang/results-master-flagO3
mkdir -p clang/results-prefetch-flagO3
mkdir -p clang/results-c-minilua-flagO3
mkdir -p clang/results-aggregate-flagO3

mkdir -p all-results/
mkdir -p all-results-csv/
mkdir -p all-results-xls/


cd TCC

#GCC c-minilua & hybrid-base & hybrid-base-prefetch
make --quiet clean > /dev/null ; make --quiet clean-hybrid > /dev/null
echo "Compiling and testing master branch (c-minilua & hybrid-base & hybrid-base-prefetch) with GCC -O2..." ; make CC='gcc' CFLAGS='--std=c11 --pedantic -Wall -Wextra -lm -O2' LLCFLAGS='-O2 -filetype=obj' && make CFLAGS='--std=c11 --pedantic -Wall -Wextra -lm -O2' LLCFLAGS='-O2 -filetype=obj' hybrid-all
../perf-test.sh ./c-minilua 15 ../gcc results-c-minilua-flagO2 c-minilua
../perf-test.sh ./hybrid-base 15 ../gcc results-master-flagO2 hybrid-base
../perf-test.sh ./hybrid-base-prefetch 15 ../gcc results-prefetch-flagO2/ hybrid-base-prefetch
echo "Done."

make --quiet clean > /dev/null ; make --quiet clean-hybrid > /dev/null
echo "Compiling and testing master branch (c-minilua & hybrid-base & hybrid-base-prefetch) with GCC -O3..." ; make CC='gcc' CFLAGS='--std=c11 --pedantic -Wall -Wextra -lm -O3' LLCFLAGS='-O3 -filetype=obj' && make CFLAGS='--std=c11 --pedantic -Wall -Wextra -lm -O3' LLCFLAGS='-O3 -filetype=obj' hybrid-all
../perf-test.sh ./c-minilua 15 ../gcc results-c-minilua-flagO3/ c-minilua
../perf-test.sh ./hybrid-base 15 ../gcc results-master-flagO3/ hybrid-base
../perf-test.sh ./hybrid-base-prefetch 15 ../gcc results-prefetch-flagO3/ hybrid-base-prefetch
echo "Done."


#CLANG c-minilua & hybrid-base & hybrid-base-prefetch
make --quiet clean > /dev/null ; make --quiet clean-hybrid > /dev/null
echo "Compiling and testing master branch (c-minilua & hybrid-base & hybrid-base-prefetch) with CLANG -O2..." ; make CC='clang' CFLAGS='--std=c11 --pedantic -Wall -Wextra -lm -O2' LLCFLAGS='-O2 -filetype=obj' && make CC='clang' CFLAGS='--std=c11 --pedantic -Wall -Wextra -lm -O2' LLCFLAGS='-O2 -filetype=obj' hybrid-all
../perf-test.sh ./c-minilua 15 ../clang results-c-minilua-flagO2 c-minilua
../perf-test.sh ./hybrid-base 15 ../clang results-master-flagO2 hybrid-base
../perf-test.sh ./hybrid-base-prefetch 15 ../clang results-prefetch-flagO2 hybrid-base-prefetch
echo "Done."

make --quiet clean > /dev/null ; make --quiet clean-hybrid > /dev/null
echo "Compiling and testing master branch (c-minilua & hybrid-base & hybrid-base-prefetch) with CLANG -O3..." ; make CC='clang' CFLAGS='--std=c11 --pedantic -Wall -Wextra -lm -O3' LLCFLAGS='-O3 -filetype=obj' && make CC='clang' CFLAGS='--std=c11 --pedantic -Wall -Wextra -lm -O3' LLCFLAGS='-O3 -filetype=obj' hybrid-all
../perf-test.sh ./c-minilua 15 ../clang results-c-minilua-flagO3/ c-minilua
../perf-test.sh ./hybrid-base 15 ../clang results-master-flagO3/ hybrid-base
../perf-test.sh ./hybrid-base-prefetch 15 ../clang results-prefetch-flagO3/ hybrid-base-prefetch
echo "Done."


cd ..
for i in `seq 1 12`;
do
    (echo "#gcc-c-minilua-O2" ; cat ./gcc/results-c-minilua-flagO2/perf-c-minilua-$i ; echo "#gcc-hybrid-base-O2" ; cat ./gcc/results-master-flagO2/perf-hybrid-base-$i ; echo "#gcc-hybrid-base-prefetch-O2" ; cat ./gcc/results-prefetch-flagO2/perf-hybrid-base-prefetch-$i) >> ./gcc/results-aggregate-flagO2/perf-$i
    (echo "#gcc-c-minilua-O3" ; cat ./gcc/results-c-minilua-flagO3/perf-c-minilua-$i ; echo "#gcc-hybrid-base-O3" ; cat ./gcc/results-master-flagO3/perf-hybrid-base-$i ; echo "#gcc-hybrid-base-prefetch-O3" ; cat ./gcc/results-prefetch-flagO3/perf-hybrid-base-prefetch-$i) >> ./gcc/results-aggregate-flagO3/perf-$i

    (echo "#clang-c-minilua-O2" ; cat ./clang/results-c-minilua-flagO2/perf-c-minilua-$i ; echo "#clang-hybrid-base-O2" ; cat ./clang/results-master-flagO2/perf-hybrid-base-$i ; echo "#clang-hybrid-base-prefetch-O2" ; cat ./clang/results-prefetch-flagO2/perf-hybrid-base-prefetch-$i) >> ./clang/results-aggregate-flagO2/perf-$i
    (echo "#clang-c-minilua-O3" ; cat ./clang/results-c-minilua-flagO3/perf-c-minilua-$i ; echo "#clang-hybrid-base-O3" ; cat ./clang/results-master-flagO3/perf-hybrid-base-$i ; echo "#clang-hybrid-base-prefetch-O3" ; cat ./clang/results-prefetch-flagO3/perf-hybrid-base-prefetch-$i) >> ./clang/results-aggregate-flagO3/perf-$i
done

for i in `seq 1 12`;
do
    cat ./gcc/results-aggregate-flagO2/perf-$i >> ./all-results/bench-$i
    cat ./gcc/results-aggregate-flagO3/perf-$i >> ./all-results/bench-$i

    cat ./clang/results-aggregate-flagO2/perf-$i >> ./all-results/bench-$i
    cat ./clang/results-aggregate-flagO3/perf-$i >> ./all-results/bench-$i
done


python ./c.py ./all-results-csv/ ./all-results-xls ./all-results


lscpu > cpu.info
DATE=`date '+%Y-%m-%d-%H-%M-%S'`
tar -czf results-$DATE.tar.gz gcc/ clang/ all-results/ all-results-csv/ cpu.info

#git checkout -f
#make && make hybrid
#./perf-test.sh ./c-minilua 1 ../results-c-minilua
#./perf-test.sh ./hybrid 1 ../results-prefetch
