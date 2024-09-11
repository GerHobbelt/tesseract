#!/bin/bash

set -eo pipefail

TESSDATA_VERSION=4.1.0

TESSDATA_DIR=./build-out/share/tessdata

LANGUAGES="eng"

cd "$TESSDATA_DIR"

for lang in $LANGUAGES; do
    echo "Downloading language: $lang"
    curl -LO --fail "https://raw.githubusercontent.com/tesseract-ocr/tessdata_best/$TESSDATA_VERSION/$lang.traineddata"
done
