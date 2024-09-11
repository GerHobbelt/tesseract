#!/bin/bash

set -eo pipefail

BASE_DIR="./build-out"

BIN_DIR=$(realpath "$BASE_DIR/bin")

# Directory containing the compiled libraries
LIB_DIR=$(realpath "$BASE_DIR/lib")

BIN_NAMES="tesseract"

# Name of the main shared library
LIB_NAMES=$(ls $LIB_DIR/*.dylib | xargs -n 1 basename)

EXTRA_RPATHS="@loader_path/../lib /opt/homebrew/lib /opt/homebrew/opt/libarchive/lib /usr/lib /System/Library/Frameworks/Accelerate.framework/Versions/Current"

for LIB_NAME in $LIB_NAMES; do
    if [[ -L "$LIB_DIR/$LIB_NAME" ]]; then
        echo "Skipping link $LIB_DIR/$LIB_NAME"
        continue
    fi

    # Change install_name to use @rpath for the main library
    install_name_tool -id "@rpath/$LIB_NAME" "$LIB_DIR/$LIB_NAME"
    for rpath in $EXTRA_RPATHS; do
        echo "Adding rpath: $rpath for lib/$LIB_NAME"

        install_name_tool -add_rpath "$rpath" "$LIB_DIR/$LIB_NAME" || true # Some dylibs are links of one another
    done

    # Update paths for each dependency
    for dependency in $(otool -L "$LIB_DIR/$LIB_NAME" | grep -o '\s/[^ ]*' | grep -v "^@"); do
        dep_basename=$(basename "$dependency")
        echo "Updating dependency: $dependency of lib/$LIB_NAME"

        # Change the dependency path to use @rpath or @loader_path
        install_name_tool -change "$dependency" "@rpath/$dep_basename" "$LIB_DIR/$LIB_NAME"
    done
done

for BIN_NAME in $BIN_NAMES; do
    for rpath in $EXTRA_RPATHS; do
        echo "Adding rpath: $rpath for bin/$BIN_NAME"

        install_name_tool -add_rpath "$rpath" "$BIN_DIR/$BIN_NAME"
    done

    # Update paths for each dependency
    for dependency in $(otool -L "$BIN_DIR/$BIN_NAME" | grep -o '\s/[^ ]*' | grep -v "^@"); do
        dep_basename=$(basename "$dependency")
        echo "Updating dependency: $dependency of bin/$BIN_NAME"

        # Change the dependency path to use @rpath or @loader_path
        install_name_tool -change "$dependency" "@rpath/$dep_basename" "$BIN_DIR/$BIN_NAME"
    done
done
