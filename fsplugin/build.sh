#!/bin/bash

set -x -e

CURRENT_PWD="$(dirname "${BASH_SOURCE[0]}")"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "don't run it if you are not using Mac OS X"
  exit -1
fi

export CC=$(xcrun -f clang)
export CXX=$(xcrun -f clang)
unset CFLAGS CXXFLAGS LDFLAGS

pushd $CURRENT_PWD

if [[ $1 == "debug" ]]; then
    CONFIG=Debug
    [[ -f CMakeCache.txt ]] || cmake -G Xcode -DCMAKE_BUILD_TYPE="$CONFIG" .
else
    CONFIG=Release
    rm -rf CMakeCache.txt CMakeFiles
    cmake -G Xcode -DCMAKE_BUILD_TYPE="$CONFIG" .
    xcodebuild clean
fi

xcodebuild -jobs "$(sysctl -n hw.ncpu)" -configuration "$CONFIG"
