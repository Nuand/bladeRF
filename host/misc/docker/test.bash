#!/usr/bin/env bash
#
# CI test orchestration for libbladeRF on Linux
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
# 
# 
# Run this script with:
# docker run --rm -i -t --privileged -v /dev/bus/usb:/dev/bus/usb <IMAGE> \
#   bash host/misc/docker/test.bash [boardtype [device specifier]]
# 
# where:
#   boardtype := "bladeRF" or "bladeRF-micro"
#   

if [ -n "$1" ]; then
    if [ "$1" == "bladeRF" ]; then
        _board="$1"
        _usb_vid="2cf0"
        _usb_pid="5246"
        _usb_manufacturer="Nuand"
        _usb_product="bladeRF"
    elif [ "$1" == "bladeRF-micro" ]; then
        _board="$1"
        _vid="2cf0"
        _pid="5250"
        _usb_manufacturer="Nuand"
        _usb_product="bladeRF 2.0"
    else
        echo "error: boardtype '$1' is not valid"
        echo "valid choices: bladeRF bladeRF-micro"
        exit 1
    fi
fi

if [ -n "$2" ]; then
    _args="-d $2"
fi

BUILD_DIR=""
if [ -d "build/host/output" ]; then
    BUILD_DIR="build/host/output"
elif [ -d "host/build/output" ]; then
    BUILD_DIR="host/build/output"
else
    echo "error: could not find a build artifacts directory"
    exit 1
fi

do_cmd() {
    # do_test command...
    [ -z "$1" ] && return 1

    __cmd="$*"

    echo "*** RUNNING: ${__cmd}"

    __cmd_result=$(${__cmd} 2>&1)
    __cmd_status=$?

    echo "*** status=${__cmd_status}, output:"
    echo "${__cmd_result}" | sed 's/.*/\.\.\. &/'
}

do_ask() {
    # do_ask expected result...
    if [ ${__cmd_status} -ne 0 ]; then
        __failure="nonzero status code: ${__cmd_status}"
        return 1
    fi

    echo "*** Expected result: $*"
    while true; do
        read -p ">>> Is the result as expected? [Y/n] " yn
        case ${yn:-y} in
            [Yy]* ) return 0;;
            [Nn]* ) __failure="result not as expected"; return 1;;
            * ) echo "Please answer yes or no.";;
        esac
    done
}

do_test() {
    # do_test "command" "expected result"
    echo ""
    echo "***********************"
    echo "*** BEGIN TEST CASE ***"
    echo "***********************"

    do_cmd "$1"
    do_ask "$2"
    __stat=$?

    if [ ${__stat} -ne 0 ]; then
        echo "--- FAILED (${1}): ${__failure}"
    else
        echo "+++ PASSED (${1})"
    fi
}

usb_field() {
    # usb_field vpi:vci fieldname
    [ -z "$1" ] || [ -z "$2" ] && return 1

    lsusb -v -d $1 | awk "/$2/ {\$1=\$2=\"\"; print substr(\$0,3)}"
}

do_usb_check() {
    # do_usb_check
    if [ -z "${_usb_vid}" ] || [ -z "${_usb_pid}" ]; then
        echo "*** Skipping USB check (_usb_vid and/or _usb_pid unset)"
        return 0
    fi

    echo ""
    echo "***********************"
    echo "*** BEGIN USB CHECK ***"
    echo "***********************"

    out=$(lsusb -d ${_usb_vid}:${_usb_pid})
    if [ -n "${out}" ]; then
        echo "+++ PASSED (lsusb device search): ${out}"
    else
        echo "--- FAILED (lsusb device search)"
    fi

    if [ -n "${_usb_manufacturer}" ]; then
        out=$(usb_field ${_usb_vid}:${_usb_pid} iManufacturer)
        if [ "${out}" == "${_usb_manufacturer}" ]; then
            echo "+++ PASSED (lsusb manufacturer check): ${out}"
        else
            echo "--- FAILED (lsusb manufacturer check): ${out}"
        fi
    fi

    if [ -n "${_usb_product}" ]; then
        out=$(usb_field ${_usb_vid}:${_usb_pid} iProduct)
        if [ "${out}" == "${_usb_product}" ]; then
            echo "+++ PASSED (lsusb product check): ${out}"
        else
            echo "--- FAILED (lsusb product check): ${out}"
        fi
    fi
}

do_usb_check

do_test "${BUILD_DIR}/libbladeRF_test_c" "libbladeRF <version>"
do_test "${BUILD_DIR}/libbladeRF_test_cpp" "libbladeRF <version>"

do_test "bladeRF-cli ${_args} -e probe" "list of all available boards"
do_test "bladeRF-cli ${_args} -e version" "versions of bladeRF-cli, libbladeRF, firmware, and FPGA"
do_test "bladeRF-cli ${_args} -e info" "serial number, VCTCXO DAC calibration, FPGA size"

do_test "${BUILD_DIR}/libbladeRF_test_digital_loopback ${_args} -l fw -c 500000000" "100.0% match"
do_test "${BUILD_DIR}/libbladeRF_test_digital_loopback ${_args} -l fpga -c 500000000" "100.0% match"
if [ "$_board" == "bladeRF-micro" ]; then
    do_test "${BUILD_DIR}/libbladeRF_test_digital_loopback ${_args} -l rfic -c 500000000" "100.0% match"
else
    echo "*** Skipping RFIC loopback test"
fi
