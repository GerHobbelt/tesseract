#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
TESSERACT_DIR=$SCRIPT_DIR/build-out

TESSDATA_VERSION=4.1.0

TESSDATA_DIR=$TESSERACT_DIR/share

LANGUAGES="eng"

# tessdata_best can also be used, but it's significantly slower
# See https://tesseract-ocr.github.io/tessdoc/Data-Files.html
TESSDATA_REPOS="${@:-tessdata}"

cd "$TESSDATA_DIR"

for repo in $TESSDATA_REPOS; do
    echo "Downloading repo: $repo"
    if [[ ! -d $repo ]]; then
        mkdir $repo
    fi
    cd $repo
    for lang in $LANGUAGES; do
        echo "Downloading language: $repo/$lang.traineddata"
        curl -LO --fail "https://raw.githubusercontent.com/tesseract-ocr/$repo/$TESSDATA_VERSION/$lang.traineddata"
    done
    cd -
done
