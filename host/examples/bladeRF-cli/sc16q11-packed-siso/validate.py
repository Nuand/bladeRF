import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import sys

def import_data(file_path):
    try:
        df = pd.read_csv(file_path, header=None, names=['I', 'Q'])

        if df.shape[1] < 2:
            raise ValueError("CSV file must contain at least two columns for I and Q values")

        # Check if the data is numeric
        if not (df['I'].dtype.kind in 'biufc' and df['Q'].dtype.kind in 'biufc'):
            raise ValueError("I and Q samples must be numeric")

        return df

    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found")
        sys.exit(1)
    except pd.errors.EmptyDataError:
        print(f"Error: File '{file_path}' is empty")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {str(e)}")
        sys.exit(1)

if len(sys.argv) != 2:
    print("Usage: python validate.py <path_to_csv_file>")
    sys.exit(1)

file_path = sys.argv[1]
df = import_data(file_path)

# Plot I and Q components on the same graph
plt.figure(figsize=(10, 6))
plt.plot(df.index, df['I'], label='I', alpha=0.7)
plt.plot(df.index, df['Q'], label='Q', alpha=0.7)
plt.xlabel('Sample Index')
plt.ylabel('Amplitude')
plt.title('I/Q Components Over Time')
plt.legend()
plt.grid(True, alpha=0.3)

plt.tight_layout()
plt.show()
