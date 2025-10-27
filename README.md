***

# STM32 Drone Telemetry System with Real-Time OLED Display

This project is an embedded telemetry decoder and real-time display solution for ArduPilot drone logs, built on the STM32F401RE microcontroller. It features reliable UART telemetry reception, visualizes flight metrics on a 0.96" I2C OLED display, and supports SD card logging via SPI for persistent data storage. The system is validated with live drone log data and is suitable for academic, prototyping, and operational use.[4]

***

## Features

- UART telemetry reception (9600 baud): Receives flight data via serial port
- I2C OLED display: Real-time visualization of altitude, speed, and voltage
- SPI SD card logging: Stores telemetry data for post-flight analysis
- CSV data pipeline: Handles ArduPilot log extraction, Python-based formatting, and embedded parsing

***

## System Status

- UART + I2C OLED fully functional with 100% parse accuracy
- Real-time telemetry display at up to 5 Hz update rate, with display latency under 100 ms
- SD card logging module implemented and in active debug/development (FatFS integration, SPI timing verification)

***

## Hardware Overview

| Component         | Model                | Notes               |
|-------------------|---------------------|---------------------|
| Microcontroller   | STM32F401RE Nucleo  | Development board   |
| Display           | SSD1306 OLED 0.96"  | I2C interface       |
| Storage           | SD Card Module      | SPI interface, 3.3V |
| Serial            | HW-558A USB-TTL     | CP2102 chip         |
| Power             | Nucleo 3.3V Regulator | Shares all modules |

***

## Project Structure

```
STM32-Drone-Telemetry-System/
├── Firmware/           STM32CubeIDE project and source code
├── Python_Scripts/     Data streaming and formatting utilities
├── Documentation/      Detailed technical reports
└── Images/             Hardware reference photographs
```

***

## Quick Start Instructions

### Firmware

1. Open STM32CubeIDE and select the project in `Firmware/`
2. Build the firmware from source (Ctrl+B)
3. Flash the binary to your STM32F401RE board via ST-LINK (Debug/Run)

### Python Telemetry Pipeline

1. Install Python 3.8+ and `pyserial` using `pip install pyserial`
2. Place formatted CSV telemetry data in `Python_Scripts/`
3. Run `telemetry_streamer.py` to transmit data via serial to STM32

***

## Documentation

- University_Mini_Project_Submission.md – Academic project report
- Drone_Telemetry_Project_Report.md – Full technical documentation
- Source_Code_Package_README.md – Firmware and hardware integration details

***

## Performance Metrics

| Metric               | Target         | Measured    |
|----------------------|---------------|-------------|
| UART Baud Rate       | 9600 bps      | 9600 bps    |
| Telemetry Update     | up to 5 Hz    | 5 Hz        |
| OLED Latency         | < 100 ms      | ~50 ms      |
| CSV Parse Accuracy   | 100%          | 100%        |
| Flash Memory Usage   | < 256 KB      | ~45 KB      |
| RAM Usage            | < 64 KB       | ~3 KB       |

***

## Testing

- UART communication validated for flight data reception and parsing accuracy
- OLED display verified for stable updates and legibility
- System clock (84 MHz) and I2C (100 kHz) confirmed as per configuration
- SD Card driver and FatFS integration under active test

***

## License

This repository is published under the MIT License. See LICENSE file for details.

***

## Author

Kavin K.K  
Electronics Engineering Student - VLSI
Rashtriya Raksha University  
Email: kavin28eng@gmail.com  
GitHub: https://github.com/Kavin-Re

***

*Submitted as a Mini-Project to Rashtriya Raksha University, October 2025.*

***
