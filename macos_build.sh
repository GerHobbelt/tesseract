#!/bin/bash

set -eo pipefail

mkdir -p build build-out

cd build

../configure --disable-openmp --prefix=$(realpath ../build-out) --disable-doc

make
make install

cd ..

./macos_link.sh
