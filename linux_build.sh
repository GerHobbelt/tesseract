#!/bin/bash

set -eo pipefail

mkdir -p build build-out

cd build

../configure \
    LDFLAGS="-Wl,-rpath,'\$\$ORIGIN/../lib' -Wl,--disable-new-dtags" \
    --disable-openmp \
    --disable-doc \
    --prefix=$(realpath ../build-out)

make
make install
