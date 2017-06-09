#!/usr/bin/fish

# Example usage:
#   ./benchmark.fish whilewhile 10

set input $argv[1]
set niter $argv[2]

set inputlua  "./examples/$input.lua"
set inputbyte "./examples/$input.byte"

for file in $inputlua $inputbyte
    if not [ -f $file ]
        echo "File $file doesn't exist"
        exit 1
    end
end

function bench
    echo "======="
    echo $argv $niter
    echo "======="
    time $argv $niter 2>&1
    echo
end

bench ./c-minilua $inputbyte
bench ./hybrid $inputbyte
bench lua           ./lua-minilua.lua $inputbyte
bench luajit -j off ./lua-minilua.lua $inputlua
bench luajit -O3    ./lua-minilua.lua $inputlua
