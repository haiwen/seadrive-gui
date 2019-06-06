#!/bin/bash

set -e

CURRENT_PWD="$(dirname "${BASH_SOURCE[0]}")"
cd $CURRENT_PWD

# qlmanage -x -c public.jpeg -g ~/Library/QuickLook/SeaDriveQL.qlgenerator -t test.jpg
qlmanage -t "$@" test.jpg
