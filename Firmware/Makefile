##############################################################################
# STM32 Drone Telemetry System - Makefile
# Target: STM32F401RE
# Toolchain: arm-none-eabi-gcc
##############################################################################

# MCU and Toolchain Variables
MCU = cortex-m4
DEVICE = STM32F401RE
CC = arm-none-eabi-gcc
LD = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE = arm-none-eabi-size

# Directory Structure
SRC_DIR = Core/Src
INC_DIR = Core/Inc
DRIVERS_DIR = Drivers
FATFS_DIR = Middlewares/FatFs/src
# Add more directories as needed

# Firmware source files
SRC = \
  $(SRC_DIR)/main.c \
  $(SRC_DIR)/stm32f4xx_it.c \
  $(SRC_DIR)/syscalls.c \
  $(SRC_DIR)/system_stm32f4xx.c \
  $(SRC_DIR)/ssd1306.c \
  $(SRC_DIR)/ssd1306_tests.c \
  $(SRC_DIR)/ssd1306_fonts.c \
  $(DRIVERS_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
  $(DRIVERS_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
  $(DRIVERS_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c \
  $(FATFS_DIR)/ff.c \
  $(FATFS_DIR)/diskio.c \
  $(FATFS_DIR)/user_diskio.c

# Include paths
INCLUDES = -I$(INC_DIR) -I$(DRIVERS_DIR)/STM32F4xx_HAL_Driver/Inc -IMiddlewares/FatFs/inc

# Build options
CFLAGS = -mcpu=$(MCU) -mthumb -Wall -g -O2 $(INCLUDES) \
  -DUSE_HAL_DRIVER -DSTM32F401xE

LDFLAGS = -TSTM32F401RETX_FLASH.ld

# Target binary names
TARGET = stm32_telemetry.elf
HEX = stm32_telemetry.hex

# Default build rule
all: $(HEX)

# Compile to ELF binary
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)
	$(SIZE) $@

# Convert ELF to HEX for flashing
$(HEX): $(TARGET)
	$(OBJCOPY) -O ihex $< $@

# Clean build artifacts
clean:
	rm -f $(TARGET) $(HEX) *.o *.d

# Print size
size:
	$(SIZE) $(TARGET)

# Download to board (assumes st-flash installed)
flash: $(HEX)
	st-flash write $(HEX) 0x8000000

.PHONY: all clean size flash
