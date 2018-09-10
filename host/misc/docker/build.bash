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

# Usage:
# build.bash [distro-version [distro-version...]]
# e.g.
# build.bash
# build.bash archlinux
# build.bash debian-jessie

# Initial assumptions...
compilers="gcc clang"
buildtypes="Release Debug Tagged"
repository="nuand/bladerf-buildenv"
make_jobs=8
make_jobs_clangscan=4

# assuming the script is 3 levels down (host/misc/docker)
dfdir=$(dirname "${0}")
basedir=${dfdir}/../../..

# globals
success=""
failure=""
rels=""

do_base () {
    # usage: do_base
    # builds the base image
    echo "*** Building ${repository}:base image..."
    docker build -f host/misc/docker/_base.Dockerfile \
                 -t ${repository}:base \
                 .
    __status=$?
}

do_build () {
    # usage: do_build rel compiler buildtype
    # performs a build with the given settings
    [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ] && return 1
    rel=$1
    compiler=$2
    buildtype=$3
    tag="${rel}-${compiler}-${buildtype}"
    taggedrelease=NO

    if [ "${rel}" == "clang-scan" ]; then
        mj=${make_jobs_clangscan}
    else
        mj=${make_jobs}
    fi

    if [ "${buildtype}" == "Tagged" ]; then
        # tagged means a Release with -DTAGGED_RELEASE=YES
        buildtype=Release
        taggedrelease=YES
    fi

    echo "*** ${tag}: building rel=${rel} compiler=${compiler} buildtype=${buildtype} taggedrelease=${taggedrelease} parallel=${mj}"

    docker build -f ${rel}.Dockerfile \
                 --build-arg buildtype=${buildtype} \
                 --build-arg compiler=${compiler} \
                 --build-arg taggedrelease=${taggedrelease} \
                 --build-arg parallel=${mj} \
                 -t ${repository}:${tag} \
                 .
    __status=$?
}

do_test () {
    # usage: do_test tag
    # runs a cursory test on the given image
    [ -z "$1" ] && return 1
    tag=$1

    echo "*** ${tag}: testing ${repository}:${tag}"

    __cliversion=$(docker run --rm -t ${repository}:${tag} bladeRF-cli --version | tr -d '\n\r')
    __libversion=$(docker run --rm -t ${repository}:${tag} bladeRF-cli --lib-version | tr -d '\n\r')
    __status=$?
}

main () {
    # usage: main rel [rel...]
    # main program body
    [ -z "$1" ] && return 1
    rels="$*"

    if [ -z "${rels}" ]; then
        echo "could not find any Dockerfiles to build!"
        exit 1
    fi

    echo "*** $0: build goals: ${rels}"
    cd ${dfdir}

    for df in ${rels}; do
        rel=$(basename -s ".Dockerfile" ${df})

        if [ "${rel}" == "clang-scan" ]; then
            # special case: this one is just a self-contained build
            tag="${rel}"
            echo "*** ${tag}: preparing to build"
            do_build clang-scan ccc-analyzer Release
            status=$?
            if [ $__status -ne 0 ]; then
                echo "*** ${tag}: BUILD FAILED"
                failure="${failure} ${tag}"
                continue
            else
                echo "*** ${tag}: build successful"
                success="${success} ${tag}"
            fi
        else
            for compiler in ${compilers}; do
                for buildtype in ${buildtypes}; do
                    # this tag uniquely identifies the build target
                    tag="${rel}-${compiler}-${buildtype}"

                    echo "*** ${tag}: preparing to build"
                    do_build ${rel} ${compiler} ${buildtype}
                    echo "*** ${tag}: status=${__status}"
                    if [ $__status -ne 0 ]; then
                        echo "*** ${tag}: BUILD FAILED"
                        failure="${failure} ${tag}"
                        continue
                    else
                        echo "*** ${tag}: build successful"
                    fi

                    echo "*** ${tag}: preparing to test"
                    do_test ${tag}
                    echo "*** ${tag}: status=${__status} cliversion=${__cliversion} libversion=${__libversion}"
                    if [ $__status -ne 0 ]; then
                        echo "*** ${tag}: TEST FAILED"
                        failure="${failure} ${tag}"
                        continue
                    else
                        echo "*** ${tag}: test successful"
                    fi

                    echo "*** ${tag}: PASSED (${__libversion})"
                    success="${success} ${tag}(${__libversion})"
                done
            done
        fi
    done

    echo "RESULT:SUCCESS:${success}"
    echo "RESULT:FAILED:${failure}"

    [ -n "${failure}" ] && exit 1
}

if [ $# -gt 0 ]; then
    # We have an argument, so just build those images...
    for rootname in "$*"; do
        if [ "${rootname}" == "_base" ]; then
            do_base
            continue
        fi
        rels="${rels} ${rootname}"
    done
else
    # No argument, so build everything
    cd ${basedir}
    do_base

    for dff in $(ls ${dfdir}/*.Dockerfile); do
        rootname=$(basename -s ".Dockerfile" ${dff})
        if [ "${rootname}" == "_base" ]; then
            continue
        fi
        rels="${rels} ${rootname}"
    done
fi

main ${rels}
