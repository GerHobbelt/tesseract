#!/bin/bash

set -eo pipefail

mkdir -p build build-out

cd build

../configure \
    --disable-openmp \
    --disable-graphics \
    --disable-doc \
    --disable-cpu-optimizations \
    --prefix=$(realpath ../build-out)

make
make install

cd ..

./macos_link.sh
