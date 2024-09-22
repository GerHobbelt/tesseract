#!/bin/bash

sed '4i\                                     
TESSERACT_DIR=$(dirname $SCRIPT_DIR)
' download_tessdata.sh > build-out/bin/download_tessdata.sh
chmod +x build-out/bin/download_tessdata.sh
