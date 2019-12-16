#!/bin/bash

set -e

CURRENT_PWD="$(dirname "${BASH_SOURCE[0]}")"
cd $CURRENT_PWD

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "don't run it if you are not using Mac OS X"
  exit -1
fi

. ./update-version.sh

export CC=$(xcrun -f clang)
export CXX=$(xcrun -f clang)
unset CFLAGS CXXFLAGS LDFLAGS

CONFIG=Release
cmake -G Xcode -DCMAKE_BUILD_TYPE="$CONFIG" .
xcodebuild clean
xcodebuild -jobs "$(sysctl -n hw.ncpu)" -configuration "$CONFIG"
