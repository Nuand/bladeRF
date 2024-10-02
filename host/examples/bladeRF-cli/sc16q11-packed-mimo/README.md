# BladeRF CLI RX MIMO Packed

This example demonstrates how to configure and use the BladeRF CLI to aquire an RX stream of packed 16-bit samples.

## Prerequisites

Before running this example, ensure the Python environment is initialized and install the dependencies:

```bash
python3 -m venv env
source env/bin/activate
pip install -r requirements.txt
```

## Running the Example

To run the example, execute the following command:

```bash
./run.sh
```

This will execute the bladeRF-cli script that ingests samples from an internal 12b counter and writes them to a CSV file.

The Python script `validate.py` will then read the CSV file and plot the samples for visual inspection.
