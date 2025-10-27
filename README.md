# STM32 Drone Telemetry System with Real-Time OLED Display

Real-time embedded telemetry decoder for ArduPilot drone logs using STM32F401RE microcontroller.

## 🎯 Features

- **UART Telemetry Reception** (9600 baud): Receive drone flight data via serial
- **I2C OLED Display**: Real-time visualization on 0.96" SSD1306 display
- **SPI SD Card Logging**: Persistent data storage using microSD card
- **CSV Data Pipeline**: ArduPilot log → Python processing → Embedded display

## 📊 Status

- ✅ UART + I2C OLED: Fully functional
- ✅ Real-time telemetry display at 5 Hz update rate
- ⚠️ SD Card logging: Debug phase (low-level SPI verification)

## 🔧 Hardware

- **MCU**: STM32F401RE Nucleo Board
- **Display**: SSD1306 0.96" OLED (I2C)
- **Storage**: SD Card Module (SPI)
- **Serial**: HW-558A USB-TTL Adapter

## 📁 Project Structure

```
STM32-Drone-Telemetry-System/
├── Firmware/          - STM32CubeIDE project
├── Python_Scripts/    - Data streaming utilities
├── Documentation/     - Technical reports
└── Images/           - Hardware photos
```

## 🚀 Quick Start

### Firmware Setup
1. Open STM32CubeIDE
2. Load project from `Firmware/` folder
3. Build and flash to board

### Python Streaming
```bash
pip install pyserial
python telemetry_streamer.py
```

## 📚 Documentation

- `University_Mini_Project_Submission.md` - Academic report
- `Drone_Telemetry_Project_Report.md` - Technical reference
- `Source_Code_Package_README.md` - Code guide

## 📊 Performance

| Metric | Value |
|--------|-------|
| UART Baud | 9600 bps |
| Update Rate | 5 Hz |
| OLED Latency | <100 ms |
| CSV Parse Accuracy | 100% |
| Memory (Flash) | 45 KB |
| Memory (RAM) | 3 KB |

## 🔬 Testing Results

- UART Communication: ✅ 100% accuracy
- OLED Display: ✅ <100ms latency
- System Clock: ✅ 84 MHz
- I2C Clock: ✅ 100 kHz

## 📄 License

MIT License - See LICENSE file for details

## 👤 Author

**Kavin.K.K**  
Electronics Engineering Student  
Trinnovate Synergy Technologies (Internship)

## 📞 Contact

Email: kavin28eng@gmail.com  
GitHub: https://github.com/Kavin-Re

---

*Submitted as Mini-Project to [Your University], October 2025*