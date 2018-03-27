#!/usr/bin/env bash

# assuming the script is 3 levels in (host/misc/docker)
dfdir=$(dirname "${0}")
basedir=${dfdir}/../../..

success=""
failure=""

rels=""

if [ $# -gt 0 ]; then
    rels=$*
else
    for dff in $(ls ${dfdir}/*.Dockerfile); do
        rels="${rels} $(basename -s ".Dockerfile" ${dff})"
    done
fi

if [ -z "${rels}" ]; then
    echo "could not find any Dockerfiles to build!"
    exit 1
fi

echo "*** $0: build goals: ${rels}"

for df in ${rels}; do
    rel=$(basename -s ".Dockerfile" ${df})
    echo "*** ${rel}: building"
    cd ${basedir}

    echo -e '\033]2;'${rel} - building'\007'

    docker build -f host/misc/docker/${rel}.Dockerfile \
                 -t nuand/bladerf-buildenv:${rel} \
                 .
    status=$?

    if [ $status -ne 0 ]; then
        echo "*** ${rel}: BUILD FAILED"
        failure="${failure} ${rel}"
        continue
    else
        echo "*** ${rel}: build successful"
    fi

    echo -e '\033]2;'${rel} - testing'\007'

    docker run --rm -t nuand/bladerf-buildenv:${rel} bladeRF-cli --version
    status=$?

    if [ $status -ne 0 ]; then
        echo "*** ${rel}: TEST FAILED"
        failure="${failure} ${rel}"
        continue
    else
        echo "*** ${rel}: test successful"
    fi

    echo "*** ${rel}: PASSED"
    success="${success} ${rel}"
done

echo "RESULTS:"
echo "Successful builds: ${success}"
echo "Failed builds: ${failure}"
