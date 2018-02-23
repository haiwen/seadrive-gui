#!/bin/bash

set -x -e

CURRENT_PWD="$(dirname "${BASH_SOURCE[0]}")"

pushd $CURRENT_PWD
rm -rf CMakeCache.txt CMakeFiles
cmake -GNinja -DCMAKE_BUILD_TYPE="Release"
ninja

if [[ $# -gt 0 ]]; then
    appdir=$1
    servicedir=${appdir}/Contents/Library/LaunchServices
    mkdir -p $servicedir
    cp -v com.seafile.seadrive.helper $servicedir
fi
