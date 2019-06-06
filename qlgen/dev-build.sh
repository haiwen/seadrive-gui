#!/bin/bash

set -e

CURRENT_PWD="$(dirname "${BASH_SOURCE[0]}")"
cd $CURRENT_PWD

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "don't run it if you are not using Mac OS X"
  exit -1
fi

err_and_quit() {
    if [[ -t 1 ]]; then
        >&2 printf "\n\n\033[33mError: %s\033[m\n\n" "$1"
    else
        >&2 echo "$1"
    fi
    exit 1
}

build() {
    . ./update-version.sh

    CONFIG=Debug
    if [[ -f CMakeCache.txt ]]; then
        full=true
    fi
    if [[ $full == "true" ]]; then
        export CC=$(xcrun -f clang)
        export CXX=$(xcrun -f clang)
        unset CFLAGS CXXFLAGS LDFLAGS
        cmake -G Xcode -DCMAKE_BUILD_TYPE="$CONFIG"
        # xcodebuild clean
    fi

    echo "Building the project ..."
    xcodebuild -jobs "$(sysctl -n hw.ncpu)" -configuration "$CONFIG"

    localdir=~/Library/QuickLook
    echo "Installing SeaDriveQL to $localdir"
    rsync -a --delete $CONFIG/SeaDriveQL.qlgenerator $localdir
    echo "Done."
    # seadrive-gui would try install SeaDriveQL.qlgenerator when starting
    appdir=../Contents/Resources/
    echo "Installing SeaDriveQL to $appdir"
    rsync -a --delete $CONFIG/SeaDriveQL.qlgenerator $appdir
    echo "Done."

    if [[ $reset == "true" ]]; then
        echo "Asking quicklookd to refresh generators"
        qlmanage -r
        echo "Done."
    fi
}

main() {
    full=false
    reset=false
    runtest=false
    while [[ $# -gt 0 ]]
    do
        case "$1" in
            # full build, re-run cmake
            -x|--test)
                runtest=true; shift 1;;
            -f|--full)
                full=true; shift 1 ;;
            -r|--reset-quicklook)
                reset=true; shift 1 ;;
            *)  err_and_quit "Argument error. Please see help." ;;
        esac
    done

    build

    if [[ $runtest == "true" ]]; then
        qlmanage -x -t test.jpg -s 32
    fi
}

main "$@"
