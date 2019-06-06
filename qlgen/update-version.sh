#!/bin/bash

cp Info.plist.template Info.plist
version_placeholder="XXXXXX"
version_real=$(date '+%Y%m%d.%H%M%S')

gsed -i -e "s/${version_placeholder}/${version_real}/g" Info.plist

