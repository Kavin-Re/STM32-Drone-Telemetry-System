# telemetry_streamer.py

import serial
import time
import csv
import sys

# --- Configuration ---
# ⚠️ Ensure this port matches the Silicon Labs CP210x port (COM3)
SERIAL_PORT = 'COM3' 
BAUD_RATE = 9600
DATA_FILE = 'telemetry_stream.csv'

# The rate at which the data rows are sent (5 Hz = 0.2 seconds delay)
UPDATE_RATE_HZ = 5.0
DELAY_TIME = 1.0 / UPDATE_RATE_HZ 

def stream_telemetry():
    """Reads the formatted CSV and streams it over the serial port."""
    
    try:
        # 1. Initialize Serial Connection
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"✅ Opened serial port {SERIAL_PORT} at {BAUD_RATE} baud.")
        print(f"🚀 Starting telemetry stream from {DATA_FILE} at {UPDATE_RATE_HZ} Hz...")
        time.sleep(1) # Wait for serial port to stabilize
        
    except serial.SerialException as e:
        print(f"❌ Error: Could not open serial port {SERIAL_PORT}.")
        print(f"   Details: {e}")
        print("   Check if the USB-TTL adapter is connected and drivers are installed.")
        sys.exit(1)

    try:
        # 2. Read and Stream Data
        with open(DATA_FILE, 'r') as f:
            reader = csv.reader(f)
            
            # Skip the header row if it exists (Our formatter suppressed it, but good practice)
            # next(reader)
            
            start_time = time.time()
            row_count = 0

            for row in reader:
                # The file is already formatted as: [Altitude, Speed, Voltage]
                data_line = ','.join(row) + '\n'
                
                # Encode the string to bytes and send
                ser.write(data_line.encode('ascii'))
                
                # Optional: Print to console for confirmation
                print(f"[{row_count}] Sent: {data_line.strip()}")
                
                row_count += 1
                
                # Maintain the 5 Hz update rate
                time.sleep(DELAY_TIME)
                
    except FileNotFoundError:
        print(f"❌ Error: Data file '{DATA_FILE}' not found.")
        print("   Ensure the streamer is run from the same folder as the CSV file.")
        
    except KeyboardInterrupt:
        print("\n🛑 Stream interrupted by user.")
        
    except Exception as e:
        print(f"\n❌ An unexpected error occurred during streaming: {e}")

    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print(f"\nSerial port {SERIAL_PORT} closed.")

if __name__ == "__main__":
    stream_telemetry()