#!/bin/bash

# Copyright (c) 2011-2021 Benjamin Fleischer
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# Requires common.sh
# Requires string.sh
# Requires version.sh


function macos_get_version
{
    sw_vers -productVersion 2> /dev/null
}

function macos_version_to_major
{
    local version="${1}"

    common_assert "version_is_version `string_escape "${version}"`"

    version_compare "${version}" "11"
    if (( ${?} == 1 ))
    then
        local fields="1,2"
    else
        local fields="1"
    fi

    cut -d "." -f ${fields} <<< ${version}
}

function macos_unload_kernel_extension
{
    local identifier="${1}"

    common_assert "[[ -n `string_escape "${identifier}"` ]]"

    if [[ -n "`/usr/sbin/kextstat -l -b "${identifier}"`" ]]
    then
        /sbin/kextunload -b "${identifier}" 1>&3 2>&4
    else
        return 0
    fi
}
