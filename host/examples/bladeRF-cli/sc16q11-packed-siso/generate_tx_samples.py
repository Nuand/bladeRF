import csv
import sys

def generate_csv_normal(filename):
    with open(filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)

        # Generate numbers from 0 to 2047
        for i in range(0, 2048, 2):
            writer.writerow([i, i+1])

        # Generate numbers from -2048 to -1
        for i in range(-2048, 0, 2):
            writer.writerow([i, i+1])

def generate_csv_cw(filename):
    cw_pattern = [
        [0, 2047],
        [2047, 0],
        [0, -2047],
        [-2047, 0]
    ]

    with open(filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        for _ in range(2*4096):
            for sample in cw_pattern:
                # Duplicate the pattern for both channels
                writer.writerow(sample)

if __name__ == "__main__":
    filename = 'tx_samples.csv'

    if len(sys.argv) > 1:
        if sys.argv[1] == "-h" or sys.argv[1] == "--help":
            print("Usage: python generate_tx_samples.py [option]")
            print("Options:")
            print("  -h, --help    Show this help message and exit")
            print("  cw            Generate CSV file with CW pattern")
            print("  (no option)   Generate CSV file with count pattern")
            sys.exit(0)
        elif sys.argv[1] == "cw":
            generate_csv_cw(filename)
            print(f"CSV file '{filename}' has been generated with CW pattern.")
        else:
            print(f"Unknown option: {sys.argv[1]}")
            print("Use -h or --help for usage information.")
            sys.exit(1)
    else:
        generate_csv_normal(filename)
        print(f"CSV file '{filename}' has been generated with count pattern.")
