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
    if [[ $full == "true" ]]; then
        export CC=$(xcrun -f clang)
        export CXX=$(xcrun -f clang)
        unset CFLAGS CXXFLAGS LDFLAGS
        cmake -GNinja .
        # xcodebuild clean
    fi

    echo "Building the cli ..."
    ninja
}

main() {
    full=false
    while [[ $# -gt 0 ]]
    do
        case "$1" in
            -f|--full)
                full=true; shift 1 ;;
            *)  break ;;
        esac
    done
    build
    set -x
    ./qlgen-cli "$@"
}

main "$@"
