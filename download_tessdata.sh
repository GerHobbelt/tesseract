#!/bin/bash

set -eo pipefail

TESSDATA_VERSION=4.1.0

TESSDATA_DIR=./build-out/share/tessdata

LANGUAGES="eng"

# tessdata_best can also be used, but it's significantly slower
# See https://tesseract-ocr.github.io/tessdoc/Data-Files.html
TESSDATA_REPO=tessdata_fast

cd "$TESSDATA_DIR"

for lang in $LANGUAGES; do
    echo "Downloading language: $TESSDATA_REPO/$lang"
    curl -LO --fail "https://raw.githubusercontent.com/tesseract-ocr/$TESSDATA_REPO/$TESSDATA_VERSION/$lang.traineddata"
done
