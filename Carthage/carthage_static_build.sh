#!/bin/bash

set -e

CURRENT_PWD="$(dirname "${BASH_SOURCE[0]}")"
cd ${CURRENT_PWD}/..

xcconfig=$(mktemp /tmp/static.xcconfig.XXXXXX)
trap 'rm -f "$xcconfig"' INT TERM HUP EXIT

echo "LD = $PWD/Carthage/static-ld.py" >> $xcconfig
echo "DEBUG_INFORMATION_FORMAT = dwarf" >> $xcconfig

export XCODE_XCCONFIG_FILE="$xcconfig"

carthage build --platform mac
