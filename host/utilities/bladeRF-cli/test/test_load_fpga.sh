#!/bin/bash
COMMAND="bladeRF-cli -l $1"

cleanup() {
    rm -f stdout.txt stderr.txt
    echo
    echo "Exiting..."
    exit 0
}

help() {
    echo "This script requires a path to an FPGA image."
    echo
    echo "Usage:    ./test_load_fpga [PATH]"
    echo "Example:  ./test_load_fpga ~/hostedxA9_latest.rbf"
    echo
    exit 0
}

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    help
fi

if [ -z "$1" ]; then
    echo "[Error] Path argument is required."
    help
    exit 1
fi

trap cleanup EXIT
trap break SIGINT SIGTERM

for ((i=1; i<=20; i++))
do
    echo "Running test $i"

    $COMMAND >stdout.txt 2>stderr.txt

    if [ -s stderr.txt ]; then
        cat stderr.txt
        echo
        echo "[Error] Test failed!"
        break
    fi

    echo "Test passed!"
done