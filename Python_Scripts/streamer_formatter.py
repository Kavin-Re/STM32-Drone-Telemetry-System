import csv
import subprocess
from datetime import timedelta

# Extract data from ArduPilot .bin log
subprocess.run(['mavlogdump.py', 'flight.bin', '--format', 'csv'], 
               stdout=open('raw_data.csv', 'w'))

# Parse telemetry fields WITH TIMESTAMPS
data_with_time = []

with open('raw_data.csv', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        try:
            # Extract timestamp (in microseconds, convert to milliseconds)
            timestamp_us = float(row.get('TimeUS', 0))  # Microseconds
            timestamp_ms = timestamp_us / 1000.0  # Convert to milliseconds
            
            # Convert to seconds and get HH:MM:SS
            total_seconds = int(timestamp_ms / 1000)
            hours = total_seconds // 3600
            minutes = (total_seconds % 3600) // 60
            seconds = total_seconds % 60
            
            # Extract telemetry fields
            if 'AHR2.Alt' in row and row['AHR2.Alt']:
                altitude = float(row['AHR2.Alt'])
            else:
                continue
            
            if 'XKF1.VN' in row and 'XKF1.VE' in row:
                vn = float(row['XKF1.VN'])
                ve = float(row['XKF1.VE'])
                speed = (vn**2 + ve**2)**0.5
            else:
                continue
            
            if 'BAT.Volt' in row and row['BAT.Volt']:
                voltage = float(row['BAT.Volt'])
            else:
                continue
            
            # Store with timestamp
            data_with_time.append({
                'timestamp_ms': timestamp_ms,
                'hours': hours,
                'minutes': minutes,
                'seconds': seconds,
                'altitude': altitude,
                'speed': speed,
                'voltage': voltage
            })
        except (ValueError, KeyError):
            continue

# Create CSV with timestamp info
with open('telemetry_stream.csv', 'w') as f:
    f.write("TimeMS,Hours,Minutes,Seconds,Altitude,Speed,Voltage\n")
    for data in data_with_time:
        f.write(f"{data['timestamp_ms']:.0f},"
                f"{data['hours']},"
                f"{data['minutes']},"
                f"{data['seconds']},"
                f"{data['altitude']:.2f},"
                f"{data['speed']:.2f},"
                f"{data['voltage']:.2f}\n")

print(f"Created telemetry_stream.csv with {len(data_with_time)} records")
