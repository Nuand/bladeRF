#!/bin/bash
set -e

echo "Creating virtual environment 'nuand'..."
python3 -m venv nuand

echo "Activating virtual environment 'nuand'..."
source nuand/bin/activate

echo "Installing Python requirements..."
pip3 install --requirement=output/requirements.txt

GREEN="\033[32m"
RESET="\033[0m"
echo -e "${GREEN}\n"
echo "###################################################"
echo "Python virtual environment successfully installed"
echo "###################################################"
echo -e "${RESET}"
echo "Usage:"
echo "$ source nuand/bin/activate"
echo "$ python output/libbladeRF_test_txrx_hwloop.py"
echo "$ deactivate"
