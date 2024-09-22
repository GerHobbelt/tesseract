#!/bin/bash

set -eo pipefail

mkdir -p build build-out

cd build

CONFIGURE_ARGS="--disable-openmp --disable-doc --disable-graphics"
if [[ -z "$ENABLE_CPU_OPTIMIZATIONS" ]]; then 
    CONFIGURE_ARGS+=" --disable-cpu-optimizations"
fi

case "$(uname -s)" in
    Linux*) 
        ../configure \
            LDFLAGS="-Wl,-rpath,'\$\$ORIGIN/../lib' -Wl,--disable-new-dtags" \
            $CONFIGURE_ARGS \
            --prefix=$(realpath ../build-out)

        make
        make install
        ;;
    Darwin*)
        ../configure \
            $CONFIGURE_ARGS \
            --prefix=$(realpath ../build-out)

        make
        make install

        cd ..
        ./macos_link.sh
        cd -
        ;;
    *)
        echo "Unsupported platform: $(uname -s)"
        exit 1
        ;;
esac

cd ..

sed '4i\                                     
TESSERACT_DIR=$(dirname $SCRIPT_DIR)
' download_tessdata.sh > build-out/bin/download_tessdata.sh
chmod +x build-out/bin/download_tessdata.sh
