#!/usr/bin/env bash
#
# CI build orchestration for libbladeRF on Linux
#
# Copyright (c) 2018 Nuand LLC
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# Enable job control
set -m

# Initial assumptions...
max_builds=4
max_load=$(nproc)
repository="nuand/bladerf-buildenv"

if [ -z "${max_load}" ]; then
    echo "nproc not found, assuming max load of 8"
    max_load=8
fi

# assuming the script is 3 levels down (host/misc/docker)
dfdir=$(dirname "${0}")
basedir=${dfdir}/../../..
buildscript=${dfdir}/build.bash

# globals
success=""
failure=""
rels=""

loadavg () {
    # usage: loadavg
    # outputs the current load average as an integer
    if [ -f "/proc/loadavg" ]; then
        awk '{print int($1+0)}' /proc/loadavg
    else
        echo 1
    fi
}

do_base () {
    # usage: do_base
    # builds the base image
    docker build -f host/misc/docker/_base.Dockerfile \
                 -t ${repository}:base \
                 .
    __status=$?
}

wait_for_jobs () {
    # usage: wait_for_jobs n
    # spins until there are fewer than n jobs active
    [ -z "$1" ] && exit 1

    while [ "$(jobs | wc -l)" -ge "${1}" ]; do
        if [ -z "${waiting}" ]; then
            echo -n "*** Pausing: job count above threshold ($(jobs | wc -l) >= ${1})"
            waiting=1
        else
            echo -n "."
        fi
        sleep 5
    done

    if [ -n "${waiting}" ]; then
        echo ""
        unset waiting
    fi
}

wait_for_load () {
    # usage: wait_for_load
    # spins until the system load is less than n
    [ -z "$1" ] && exit 1

    while [ "$(loadavg)" -ge "${1}" ]; do
        if [ -z "${waiting}" ]; then
            echo -n "*** Pausing: load average above threshold ($(loadavg) >= ${1})"
            waiting=1
        else
            echo -n "."
        fi
        sleep 5
    done

    if [ -n "${waiting}" ]; then
        echo ""
        unset waiting
    fi
}

do_spawn () {
    # usage: do_spawn dockerfile...
    # spawns a subprocess to execute a build, being mindful of limits
    [ -z "$1" ] && exit 1
    dff=$1
    rel=$(basename -s ".Dockerfile" ${dff})

    wait_for_jobs ${max_builds}
    wait_for_load ${max_load}

    bash ${buildscript} ${rel} 2>&1 > ${LOGDIR}/${rel}.txt &
    __pid=$!
    echo "*** Spawned ${rel} pid ${__pid}. Jobs: $(jobs | wc -l), loadavg: $(loadavg)"
}


LOGDIR=$(mktemp -d)
[ -z "${LOGDIR}" ] && echo "Couldn't create a tempdir for logs" && exit 1

echo "*** log dir: ${LOGDIR}"

cd ${basedir}
echo "*** Running initial build job: _base"
do_base 2>&1 > ${LOGDIR}/_base.txt

if [ "${__status}" -ne 0 ]; then
    echo "*** _base build failed, can't continue"
    cat ${LOGDIR}/_base.txt
    exit 1
fi

for dff in $(ls ${dfdir}/*.Dockerfile); do
    rootname=$(basename -s ".Dockerfile" ${dff})
    if [ "${rootname}" == "_base" ]; then
        continue
    fi

    do_spawn ${dff}
    WAITPID="${WAITPID} $__pid"

    # sleep for a bit to provide settling time
    sleep 5
done

echo "*** Waiting for jobs to complete"
wait ${WAITPID}

echo "*** Build logs:"
for f in ${LOGDIR}/*.txt; do
    mylog="$(basename -s .txt ${f})"
    echo "*** BEGIN LOGS for ${mylog}"
    sed "s/.*/[${mylog}] &/" ${f}
done

echo -n "*** Successful: "
grep -h "^RESULT:SUCCESS:" ${LOGDIR}/*.txt | sed 's/RESULT:SUCCESS://' | xargs

echo -n "*** Failed: "
grep -h "^RESULT:FAILED:" ${LOGDIR}/*.txt | sed 's/RESULT:FAILED://' | xargs

rm -r ${LOGDIR}
