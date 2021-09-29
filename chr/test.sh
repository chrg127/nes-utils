#!/bin/bash
test_file() {
    f=$1
    n=$2
    ./debug/chrconvert "$f.chr" -o "$f.png"
    ./debug/chrextract "$f.png" -o "$f.2.chr"
    if [[ $(diff "$f.chr" "$f.2.chr") ]]; then
        echo "test" $n "failed"
    fi
    rm "$f.png"
    rm "$f.2.chr"
}

test_file "test/testbg" 1
