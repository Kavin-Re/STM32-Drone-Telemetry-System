Below is your README text **with all emojis removed**, making it cleaner and more professional for academic and open-source use.

***

# STM32 Drone Telemetry System - Source Code Package
## Complete Firmware and Data Pipeline Implementation

**Project:** Real-Time Drone Telemetry Decoder with OLED Display and SD Logging  
**Platform:** STM32F401RE Nucleo Board  
**Date:** October 27, 2025  
**Status:** Core Functionality Operational | SD Debug Phase

***

## Package Contents

```
STM32_Telemetry_System/
├── Firmware/
│   ├── main.c                 (Core application logic)
│   ├── user_diskio.c           (SD card SPI driver for FatFS)
│   └── STM32 Telemetry.ioc     (Hardware configuration file)
├── Python_Scripts/
│   ├── telemetry_streamer.py   (Serial data sender script)
│   └── streamer_formatter.py   (ArduPilot log processor)
├── Documentation/
│   ├── Project_Report.md       (Complete technical report)
│   ├── Hardware_Schematic.pdf  (Pin mapping and wiring)
│   └── Testing_Results.txt     (Validation logs)
└── README.md                   (This file)
```

***

## Quick Start Guide

### Prerequisites

**Hardware:**
- STM32 Nucleo-F401RE development board
- SSD1306 0.96" OLED display (I2C)
- SD card module (SPI interface)
- HW-558A USB-TTL adapter (CP2102)
- Breadboard + jumper wires
- MicroSD card (FAT32 formatted, 2-32 GB)

**Software:**
- STM32CubeIDE 1.15.x or later
- Python 3.8+ with pyserial library
- ArduPilot mavlogdump.py tool (optional, for log processing)

***

## Installation Steps

### Part 1: Firmware Deployment

1. **Open Project in STM32CubeIDE:**
   ```
   File → Open Projects from File System
   Select: STM32_Telemetry_System/Firmware/
   ```
2. **Verify Configuration:**
   - Double-click `STM32 Telemetry.ioc` to view pin assignments
   - Confirm peripherals match your hardware:
     - USART1: PA9 (TX), PA10 (RX) @ 9600 baud
     - I2C1: PB8 (SCL), PB9 (SDA) @ 100 kHz
     - SPI1: PB3 (CLK), PB4 (MISO), PB5 (MOSI) @ Prescaler /2
     - GPIO: PB10 (SD CS)
3. **Build Firmware:**
   ```
   Project → Build Project (Ctrl+B)
   ```
4. **Flash to Board:**
   ```
   Run → Debug As → STM32 Cortex-M C/C++ Application (F11)
   ```

### Part 2: Python Environment Setup

1. **Install Required Libraries:**
   ```bash
   pip install pyserial
   ```
2. **Identify COM Port:**
   - Windows: Check Device Manager → Ports (COM & LPT)
   - Linux/Mac: Check `/dev/ttyUSB*` or `/dev/tty.usbserial*`
3. **Update COM Port in Script:**
   ```python
   # telemetry_streamer.py, Line 12
   SERIAL_PORT = 'COM3'  # Change to your actual port
   ```
4. **Prepare Test Data:**
   - Place `telemetry_stream.csv` in same directory as script
   - Format: `Altitude(m),Speed(km/h),Voltage(V)\n`

***

## Firmware Architecture

### 1. Main Application Flow (`main.c`)

```c
main() {
    HAL_Init();
    SystemClock_Config();
    
    // Initialize peripherals
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_SPI1_Init();
    MX_FATFS_Init();
    
    // SD CS deselect
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    
    // OLED initialization
    ssd1306_Init();
    
    // SD card mount attempt
    Mount_SD_Card();
    
    // Main loop
    while(1) {
        Telemetry_ReceiveAndParse(); // UART reception + CSV parsing
        HAL_Delay(10);
    }
}
```

### 2. Telemetry Data Structure

```c
typedef struct {
    float altitude;      // Meters (m)
    float speed;         // Kilometers per hour (km/h)
    float voltage;       // Volts (V)
    uint32_t timestamp;  // Milliseconds (ms)
} TelemetryData_t;
```

### 3. Core Functions

#### UART Reception and CSV Parsing

```c
void Telemetry_ReceiveAndParse(void) {
    // Receive one byte with timeout
    if (HAL_UART_Receive(&huart1, &rx_char, 1, 100) == HAL_OK) {
        if (rx_char == '\n') {
            // Line complete → Parse CSV
            token = strtok((char*)rx_buffer, ",");
            altitude = atof(token);
            
            token = strtok(NULL, ",");
            speed = atof(token);
            
            token = strtok(NULL, ",");
            voltage = atof(token);
            
            // Update display and log
            Telemetry_Display(&g_telemetry_data);
            Telemetry_Log(&g_telemetry_data);
        }
    }
}
```

#### OLED Display Update

```c
void Telemetry_Display(TelemetryData_t *data) {
    ssd1306_Fill(Black);
    
    ssd1306_SetCursor(0, 0);
    sprintf(lineBuffer, "ALT: %.2f m", data->altitude);
    ssd1306_WriteString(lineBuffer, Font_7x10, White);
    
    ssd1306_SetCursor(0, 16);
    sprintf(lineBuffer, "SPD: %.1f km/h", data->speed);
    ssd1306_WriteString(lineBuffer, Font_7x10, White);
    
    ssd1306_SetCursor(0, 32);
    sprintf(lineBuffer, "BAT: %.3f V", data->voltage);
    ssd1306_WriteString(lineBuffer, Font_7x10, White);
    
    ssd1306_UpdateScreen();
}
```

#### SD Card Logging

```c
void Telemetry_Log(TelemetryData_t *data) {
    if (!is_mounted) return;
    
    sprintf(logBuffer, "%lu,%.2f,%.1f,%.3f\n",
            data->timestamp, data->altitude, data->speed, data->voltage);
    
    f_open(&fil, "telemetry.csv", FA_OPEN_APPEND | FA_WRITE);
    f_write(&fil, logBuffer, strlen(logBuffer), &bytes_written);
    f_sync(&fil);
    f_close(&fil);
}
```

***

## SD Card Driver Implementation (`user_diskio.c`)

### Key Features

1. **Dynamic Speed Switching:**
   - Initialization Phase: SPI Prescaler /256 (~62.5 kHz)
   - Data Transfer Phase: SPI Prescaler /2 (8 MHz)

2. **Complete SD Command Set:**
   - CMD0: GO_IDLE_STATE
   - CMD8: SEND_IF_COND (SDv2 detection)
   - CMD17/CMD18: READ_SINGLE_BLOCK / READ_MULTIPLE_BLOCK
   - CMD24/CMD25: WRITE_BLOCK / WRITE_MULTIPLE_BLOCK
   - CMD55 + ACMD41: SD initialization sequence
   - CMD58: READ_OCR (card type detection)

3. **Card Type Support:**
   - SDv1 (Standard Capacity)
   - SDv2 (Standard Capacity)
   - SDHC/SDXC (High Capacity)
   - MMCv3 (legacy)

### SD Initialization Sequence

```c
DSTATUS USER_initialize(BYTE pdrv) {
    // 1. Low-speed init (Prescaler /256)
    hspi1.Instance->CR1 |= SPI_BAUDRATEPRESCALER_256;
    
    // 2. Power-up sequence (80 clock cycles)
    for (n = 10; n; n--) spi_xmit_byte(0xFF);
    
    // 3. CMD0: Enter idle state
    sd_send_cmd(CMD0, 0);
    
    // 4. CMD8: Check SDv2 support
    if (sd_send_cmd(CMD8, 0x1AA) == 1) {
        // SDv2 detected → ACMD41 initialization
    }
    
    // 5. Switch to high-speed (Prescaler /2)
    hspi1.Instance->CR1 |= SPI_BAUDRATEPRESCALER_2;
    
    return 0; // Success
}
```

***

## Python Data Pipeline

### Script 1: `telemetry_streamer.py`

**Purpose:** Streams telemetry data from CSV file to STM32 via serial

**Key Features:**
- Configurable update rate (default 5 Hz)
- COM port auto-detection support
- Error handling for missing files/ports
- Real-time console logging

**Usage:**
```bash
python telemetry_streamer.py
```

**Expected Output:**
```
Opened serial port COM3 at 9600 baud.
Starting telemetry stream from telemetry_stream.csv at 5.0 Hz...
[0] Sent: 0.5,2.3,12.6
[1] Sent: 1.0,2.5,12.5
[2] Sent: 1.5,3.2,12.4
...
```

### Script 2: `streamer_formatter.py` (Optional)

**Purpose:** Extracts and formats data from ArduPilot `.bin` logs

**Workflow:**
1. Extract raw data using `mavlogdump.py`
2. Parse altitude (AHR2.Alt), velocity (XKF1.VN, XKF1.VE), voltage (BAT.Volt)
3. Calculate ground speed: √(VN² + VE²)
4. Merge staggered data rows
5. Output clean `telemetry_stream.csv`

***

## Testing and Validation

### Test Case 1: UART Communication

**Setup:**
- Python script sending: `25.3,12.5,12.2\n`
- Expected STM32 parsing: `alt=25.3, speed=12.5, voltage=12.2`

**Verification:**
```c
// In main.c, add debug output:
char debug[50];
sprintf(debug, "Parsed: %.2f,%.1f,%.3f\n", altitude, speed, voltage);
HAL_UART_Transmit(&huart1, (uint8_t*)debug, strlen(debug), 100);
```

**Result:** 100% parse accuracy confirmed

### Test Case 2: OLED Display

**Setup:**
- Stream data at 5 Hz
- Monitor OLED update latency

**Expected Display:**
```
ALT: 25.30 m
SPD: 12.5 km/h
BAT: 12.200 V
LOGGING: OK
```

**Result:** Display updates in ~50ms (well under 100ms target)

### Test Case 3: SD Card Logging

**Status:** Debugging in progress

**Current Issue:**
```
[SD] f_mount() returned error code: 1 (FR_DISK_ERR)
```

**Debug Steps:**
1. Verify SPI clock prescaler (should be /256 during init)
2. Format SD card as FAT32 (not exFAT)
3. Add serial debug output to track FatFS errors
4. Test with known-good SD card

***

## Troubleshooting Guide

### Issue 1: "LOGGING: FAIL" on OLED

**Symptom:** SD card not mounting

**Solutions:**
1. Check SPI wiring (CLK on D3, MISO on D5, MOSI on D4, CS on D6)
2. Format SD card:
   - Right-click → Format
   - File system: FAT32
   - Uncheck "Quick Format"
3. Reduce SPI clock speed:
   ```c
   // In MX_SPI1_Init(), change:
   hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
   ```
4. Add debug serial output:
   ```c
   sprintf(debug, "f_mount: %d\n", f_res);
   HAL_UART_Transmit(&huart1, (uint8_t*)debug, strlen(debug), 100);
   ```

### Issue 2: UART Not Receiving Data

**Symptom:** OLED shows only zeros or old data

**Solutions:**
1. Verify COM port in Python script matches hardware
2. Check TX/RX wiring:
   - HW-558A TX → STM32 D2 (PA10 RX)
   - HW-558A RX → STM32 D8 (PA9 TX)
3. Confirm baud rate: 9600 in both Python and firmware
4. Test with serial monitor (PuTTY/Arduino Serial Monitor)

### Issue 3: OLED Display Blank

**Symptom:** No text visible on OLED

**Solutions:**
1. Check I2C address (default 0x3C for SSD1306)
2. Verify I2C wiring:
   - OLED SCL → D15 (PB8)
   - OLED SDA → D14 (PB9)
3. Test I2C communication:
   ```c
   if (HAL_I2C_IsDeviceReady(&hi2c1, 0x3C << 1, 3, 100) == HAL_OK) {
       // OLED detected
   }
   ```

### Issue 4: Compilation Errors

**Symptom:** Missing `sprintf` float support

**Solution:**
```
Project Properties → C/C++ Build → Settings → 
MCU GCC Linker → Miscellaneous → Add:
-u _printf_float
```

***

## Performance Metrics

| Metric                  | Target     | Actual    | Status   |
|-------------------------|------------|-----------|----------|
| UART Baud Rate          | 9600 bps   | 9600 bps  | Yes      |
| Update Rate             | 1-5 Hz     | 5 Hz      | Yes      |
| OLED Refresh            | < 100 ms   | ~50 ms    | Yes      |
| CSV Parse Accuracy      | 100%       | 100%      | Yes      |
| Memory Usage (Flash)    | < 256 KB   | ~45 KB    | Yes      |
| Memory Usage (RAM)      | < 64 KB    | ~3 KB     | Yes      |
| SD Logging              | Operational| Debug phase| Partial  |

***

## Code Quality Metrics

- Total Lines of Code: ~600 (main.c + user_diskio.c)
- Function Count: 12 major functions
- Cyclomatic Complexity: Low (avg 3-5 per function)
- Comment Density: 25% (well-documented)
- Compiler Warnings: 0
- Code Reusability: High (modular design)

***

## Future Enhancements

### Phase 1: SD Card Completion (1 hour)
- [ ] Finalize SD initialization debugging
- [ ] Verify FatFS error codes
- [ ] Test with multiple SD card brands

### Phase 2: Extended Telemetry (Optional)
- [ ] Add GPS coordinates (latitude, longitude)
- [ ] Display flight mode (Manual/Stabilize/Auto)
- [ ] Include battery percentage calculation

### Phase 3: Performance Optimization
- [ ] Implement DMA for SPI transactions
- [ ] Use interrupt-driven UART reception
- [ ] Reduce OLED update latency

### Phase 4: Wireless Telemetry
- [ ] Add Bluetooth module (HC-05)
- [ ] Implement live PC monitoring dashboard
- [ ] Mobile app integration (Android/iOS)

***

## References and Dependencies

### Hardware Datasheets
- STM32F401RE: https://www.st.com/resource/en/datasheet/stm32f401re.pdf
- SSD1306 OLED: https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
- CP2102 USB-UART: https://www.silabs.com/documents/public/data-sheets/CP2102-9.pdf

### Software Libraries
- STM32 HAL Library: Included in STM32CubeF4 V1.28.3
- FatFS: http://elm-chan.org/fsw/ff/
- SSD1306 Driver: https://github.com/afiskon/stm32-ssd1306
- PySerial: https://pyserial.readthedocs.io/

### Tools
- STM32CubeIDE: https://www.st.com/en/development-tools/stm32cubeide.html
- ArduPilot Tools: https://github.com/ArduPilot/pymavlink

***

## Author and Contact

**Project Lead:** Electronics Engineering Student  
**Institution:** Trinnovate Synergy Technologies (Research Internship)  
**Location:** Coimbatore, Tamil Nadu, India  
**Date:** October 2025

**Project Repository:** [Internal - Trinnovate Synergy]  
**Documentation:** Complete technical report available in `Documentation/Project_Report.md`

***

## License and Usage

This code is developed for educational and research purposes as part of an internship project.

**Usage Terms:**
- Educational use and learning
- Personal non-commercial projects
- Reference for similar embedded systems
- Commercial use without permission
- Reproduction without attribution

**Attribution:**
If you use or reference this code, please cite:
```
STM32 Drone Telemetry Decoder System
Kavin.K.K, October 2025
Electronics Engineering MINI Project
```

***

## Acknowledgments

- STMicroelectronics for comprehensive HAL documentation
- ArduPilot Community for mavlogdump.py tool and log format specifications
- Open Source Community for SSD1306 and FatFS libraries

***

**Last Updated:** October 27, 2025  
**Version:** 1.0 (Core Functional Release)  
**Status:** Production-Ready (UART/I2C) | SD Debug Phase

***

