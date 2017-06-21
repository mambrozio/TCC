#!/bin/bash
set -e
if [ "$2" == "c" ] ; then
    echo "Compiling c-minilua..."
    make clean > /dev/null 2>&1
    make > /dev/null 2>&1
    echo ""
fi

executable=$1
n=0
nerrors=0
errorFilenames=()

for filename in ./examples/*.byte; do
    if [[ "${filename}" != @("./examples/fizzbuzz.byte"|"./examples/minimize-int.byte"|"./examples/minimize-real.byte"|"./examples/odd_even.byte"|"./examples/relop.byte"|"./examples/sum-of-f.byte") ]]; then
       
        n=$(($n + 1))
        
	set -e
        ./$executable "$filename" || true #> /dev/null 2>&1
        ret=$?
	echo $ret
	set +e
        
        if [ $ret -ne 0 ]; then
            #echo "Error while running \"$filename\"" >&2
            echo -e "\e[91mError:\t\"$filename\"\e[0m" >&2
            errorFilenames+=("$filename")
            nerrors=$(($nerrors + 1))
        else
            echo -e "\e[92mPassed\e[0m:\t\e[93m\"$filename\"\e[0m"
        fi

    fi
done

echo ""
echo "Summary: "
echo "  (Some files were not tested. See list and motive below)"
echo "  Number of files tested: $n"
echo -e "  Number of files passed: \e[92m$(($n - $nerrors))\e[0m"
echo -e "  Number of files errors: \e[91m$(($nerrors))\e[0m"
if [ $nerrors != 0 ] ; then
    echo "  Files with errors: "
    for file in ${errorFilenames[@]}; do
            echo "    $file"
    done
fi
echo ""
echo "Files that do not run on c-minilua because of not implemented functions: "
echo "  6 files: "
echo "    ./examples/fizzbuzz.byte"
echo "    ./examples/minimize-int.byte"
echo "    ./examples/minimize-real.byte"
echo "    ./examples/odd_even.byte"
echo "    ./examples/relop.byte"
echo "    ./examples/sum-of-f.byte"

