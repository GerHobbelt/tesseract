#! /bin/bash

BIN2CPP=../../../../platform/win32/bin2cpp.exe 

pushd $( dirname $0 )

rm -rf out
mkdir out
$BIN2CPP --dir=src  --managerfile=HtmlResourceFileManager.h --output=out 

popd

