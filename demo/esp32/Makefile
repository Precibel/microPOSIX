# microPOSIX ESP32 Demo - Standalone Makefile
# This Makefile can be used for testing the ESP32 HAL on a desktop
# For actual ESP32 builds, use the CMakeLists.txt with ESP-IDF

PROFILE ?= 1
TARGET ?= esp32

# Compiler (use gcc for desktop testing)
CC = gcc
AR = ar
LD = ld

# Compiler flags
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O2
CFLAGS += -DMICROPOSIX_PROFILE=$(PROFILE)
CFLAGS += -DMICROPOSIX_ESP32=1
CFLAGS += -DMICROPOSIX_ARCH_XTENSA=1
CFLAGS += -DESP_PLATFORM=1

# Include paths
INCLUDES = -I../../include
INCLUDES += -I../../include/microposix
INCLUDES += -I../../include/microposix/kernel
INCLUDES += -I../../include/microposix/mm
INCLUDES += -I../../include/microposix/debug
INCLUDES += -I../../include/microposix/hal
INCLUDES += -I../../include/microposix/hal/esp32
INCLUDES += -I../../include/microposix/ble
INCLUDES += -I../../include/microposix/bootloader
INCLUDES += -I.

CFLAGS += $(INCLUDES)

# Source files
SRC_KERNEL = ../../src/kernel/scheduler.c \
             ../../src/kernel/thread.c \
             ../../src/kernel/ipc.c \
             ../../src/kernel/timer.c \
             ../../src/kernel/mq.c \
             ../../src/kernel/abi.c \
             ../../src/kernel/watchdog.c

SRC_MM = ../../src/mm/tlsf.c \
         ../../src/mm/pool.c

SRC_DEBUG = ../../src/debug/log.c \
            ../../src/debug/shell.c \
            ../../src/debug/fault.c

SRC_BLE = ../../src/ble/ble_mgr.c
SRC_BOOT = ../../src/bootloader/boot.c

# Platform-specific sources (ESP32)
SRC_PLATFORM = ../../platform/esp32/context_switch.c \
               ../../platform/esp32/cpu.c \
               ../../platform/esp32/gpio.c \
               ../../platform/esp32/uart.c \
               ../../platform/esp32/wdt.c \
               ../../platform/esp32/mpu.c

# Demo application sources
SRC_DEMO = main.c app_blink.c

# Object files
OBJ_KERNEL = $(SRC_KERNEL:.c=.o)
OBJ_MM = $(SRC_MM:.c=.o)
OBJ_DEBUG = $(SRC_DEBUG:.c=.o)
OBJ_BLE = $(SRC_BLE:.c=.o)
OBJ_BOOT = $(SRC_BOOT:.c=.o)
OBJ_PLATFORM = $(SRC_PLATFORM:.c=.o)
OBJ_DEMO = $(SRC_DEMO:.c=.o)

# Targets
TARGET_LIB = libmicroposix_esp32.a
TARGET_APP = esp32_demo

# Default target
all: $(TARGET_APP)

# Build the library
$(TARGET_LIB): $(OBJ_KERNEL) $(OBJ_MM) $(OBJ_DEBUG) $(OBJ_BLE) $(OBJ_BOOT) $(OBJ_PLATFORM)
	$(AR) rcs $@ $^

# Build the demo application
$(TARGET_APP): $(TARGET_LIB) $(OBJ_DEMO)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(OBJ_KERNEL) $(OBJ_MM) $(OBJ_DEBUG) $(OBJ_BLE) $(OBJ_BOOT) $(OBJ_PLATFORM) $(OBJ_DEMO)
	rm -f $(TARGET_LIB) $(TARGET_APP)

# Run the demo (desktop testing only)
run: $(TARGET_APP)
	./$(TARGET_APP)

# Test target
test: $(TARGET_APP)
	./$(TARGET_APP)

.PHONY: all clean run test
