set -e
bladeRF-cli -s rx.txt
python3 validate.py /tmp/data.csv
