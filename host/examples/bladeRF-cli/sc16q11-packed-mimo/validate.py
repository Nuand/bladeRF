import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import sys
import ast

def import_data(file_path):
    try:
        # Read the CSV file without header
        df = pd.read_csv(file_path, header=None)

        # Function to safely convert string representations of tuples to actual tuples
        def safe_eval(x):
            try:
                return ast.literal_eval(x)
            except:
                return x

        # Apply safe_eval to all elements
        df = df.apply(lambda col: col.map(safe_eval))

        # Rename columns based on the number of columns
        if df.shape[1] == 2:
            df.columns = ['I', 'Q']
        elif df.shape[1] == 4:
            df.columns = ['I1', 'Q1', 'I2', 'Q2']
        else:
            raise ValueError("CSV file must contain either two columns (single channel) or four columns (dual channel)")

        # Extract numeric values from tuples if necessary
        for col in df.columns:
            if df[col].dtype == 'object':
                df[col] = df[col].apply(lambda x: x[0] if isinstance(x, (tuple, list)) else x)

        # Convert to numeric, coercing errors to NaN
        df = df.apply(pd.to_numeric, errors='coerce')

        # Drop any rows with NaN values
        df = df.dropna()

        # Check if all columns are numeric
        for col in df.columns:
            if not pd.api.types.is_numeric_dtype(df[col]):
                raise ValueError(f"{col} samples must be numeric")

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

# Determine if it's single or dual channel
is_dual_channel = 'I2' in df.columns

plt.figure(figsize=(12, 8))

if is_dual_channel:
    # Plot Channel 1
    plt.subplot(2, 1, 1)
    plt.plot(df.index, df['I1'], label='I (Ch1)', alpha=0.7)
    plt.plot(df.index, df['Q1'], label='Q (Ch1)', alpha=0.7)
    plt.ylabel('Amplitude (Ch1)')
    plt.title('I/Q Components Over Time - Channel 1')
    plt.legend(loc='upper right')
    plt.grid(True, alpha=0.3)

    # Plot Channel 2
    plt.subplot(2, 1, 2)
    plt.plot(df.index, df['I2'], label='I (Ch2)', alpha=0.7)
    plt.plot(df.index, df['Q2'], label='Q (Ch2)', alpha=0.7)
    plt.xlabel('Sample Index')
    plt.ylabel('Amplitude (Ch2)')
    plt.title('I/Q Components Over Time - Channel 2')
    plt.legend(loc='upper right')
    plt.grid(True, alpha=0.3)
else:
    # Plot Single Channel
    plt.plot(df.index, df['I'], label='I', alpha=0.7)
    plt.plot(df.index, df['Q'], label='Q', alpha=0.7)
    plt.xlabel('Sample Index')
    plt.ylabel('Amplitude')
    plt.title('I/Q Components Over Time - Single Channel')
    plt.legend(loc='upper right')
    plt.grid(True, alpha=0.3)

plt.tight_layout()
plt.show()
