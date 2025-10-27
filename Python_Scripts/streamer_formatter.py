# streamer_formatter.py
import csv
import math
import pandas as pd

INPUT_FILE = 'telemetry.csv'
OUTPUT_FILE = 'telemetry_stream.csv'

try:
    # 1. Load the multi-column CSV
    df = pd.read_csv(INPUT_FILE)

    # 2. Forward-fill NaN values to merge staggered data (e.g., carry Alt forward until next Alt message)
    # This is critical because messages (like BAT and AHR2) arrive at different times.
    df['AHR2.Alt'] = df['AHR2.Alt'].ffill()
    df['XKF1.VN'] = df['XKF1.VN'].ffill()
    df['XKF1.VE'] = df['XKF1.VE'].ffill()
    df['BAT.Volt'] = df['BAT.Volt'].ffill()

    # 3. Calculate Ground Speed (GSpd = sqrt(VN^2 + VE^2))
    df['Speed'] = df.apply(lambda row: math.sqrt(row['XKF1.VN']**2 + row['XKF1.VE']**2), axis=1)

    # 4. Filter out initial rows that still contain NaNs after ffill
    df_cleaned = df.dropna(subset=['AHR2.Alt', 'Speed', 'BAT.Volt'])

    # 5. Select and format the final three columns expected by the STM32
    # Ensure floats are formatted correctly for the STM32's parsing functions
    df_stream = df_cleaned.assign(
        Altitude = df_cleaned['AHR2.Alt'].round(2).astype(str),
        Speed_kph = df_cleaned['Speed'].round(1).astype(str),
        Voltage = df_cleaned['BAT.Volt'].round(3).astype(str)
    )[['Altitude', 'Speed_kph', 'Voltage']]

    # Write the final, stream-ready CSV
    df_stream.to_csv(OUTPUT_FILE, index=False, header=False)

    print(f" Successfully created {OUTPUT_FILE} with {len(df_stream)} rows.")
    
except FileNotFoundError:
    print(f" Error: {INPUT_FILE} not found. Ensure the mavlog2csv command ran successfully.")
except Exception as e:
    print(f" An unexpected error occurred during formatting: {e}")