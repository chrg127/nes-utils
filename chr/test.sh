#!/bin/bash
test_file() {
    f=$1
    n=$2
    bpp=$3
    datamode=$4
    ./debug/chrconvert "$f.chr" -o "$f.png" -b $bpp -d $datamode
    ./debug/chrconvert -r "$f.png" -o "$f.2.chr" -b $bpp -d $datamode
    if [[ $(diff "$f.chr" "$f.2.chr") ]]; then
        echo "test" $n "failed"
    fi
    rm "$f.png"
    rm "$f.2.chr"
}

test_file_reverse() {
    f=$1
    n=$2
    bpp=$3
    ./debug/chrconvert -r "$f.png" -o "$f.chr" -b $bpp -d $datamode
    ./debug/chrconvert "$f.chr" -o "$f.2.png" -b $bpp -d $datamode
    if [[ $(diff "$f.png" "$f.2.png") ]]; then
        echo "test" $n "failed"
    fi
    rm "$f.chr"
    rm "$f.2.png"
}

test_file "test/bpp2" 1 2 planar
test_file "test/bpp4" 2 4 interwined
# test_file_reverse "test/tile" 3 2
