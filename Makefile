# microPOSIX Makefile
# 
# This Makefile supports multiple platforms:
# - ARM Cortex-M4 (CC2755)
# - ARM Cortex-M0+ (CC2340R5)
# - RISC-V (rv32imac)
# - ESP32 (Xtensa/RISC-V)
# - POSIX (for testing)

# Configuration
PROFILE ?= 1
PLATFORM ?= arm
TARGET ?= arm-none-eabi

# Detect platform
ifeq ($(PLATFORM), esp32)
TARGET := xtensa-esp32-elf
CFLAGS += -DMICROPOSIX_ESP32=1 -DMICROPOSIX_PLATFORM_ESP32=1
ifeq ($(ESP32_TARGET), esp32)
CFLAGS += -DMICROPOSIX_ARCH_XTENSA=1
else
CFLAGS += -DMICROPOSIX_ARCH_RISCV=1
endif
else ifeq ($(PLATFORM), arm)
TARGET := arm-none-eabi
CFLAGS += -DMICROPOSIX_PLATFORM_ARM=1
else ifeq ($(PLATFORM), riscv)
TARGET := riscv32-unknown-elf
CFLAGS += -DMICROPOSIX_PLATFORM_RISCV=1
else ifeq ($(PLATFORM), posix)
TARGET := 
CFLAGS += -DMICROPOSIX_PLATFORM_POSIX=1
endif

# Compiler
CC = $(TARGET)-gcc
AR = $(TARGET)-ar
LD = $(TARGET)-ld
OBJCOPY = $(TARGET)-objcopy
OBJDUMP = $(TARGET)-objdump
SIZE = $(TARGET)-size

# Compiler flags
CFLAGS += -Wall -Wextra -Werror -std=c11

# Profile-specific flags
ifeq ($(PROFILE), 1)
# Full profile (90MHz+)
CFLAGS += -DMICROPOSIX_PROFILE=1
CFLAGS += -DMICROPOSIX_MAX_THREADS=32
CFLAGS += -DMICROPOSIX_STACK_SIZE_DEFAULT=1024
CFLAGS += -DMICROPOSIX_TICK_RATE_HZ=1000
else
# Minimal profile (20MHz)
CFLAGS += -DMICROPOSIX_PROFILE=0
CFLAGS += -DMICROPOSIX_MAX_THREADS=8
CFLAGS += -DMICROPOSIX_STACK_SIZE_DEFAULT=512
CFLAGS += -DMICROPOSIX_TICK_RATE_HZ=100
endif

# Architecture-specific flags
ifeq ($(PLATFORM), arm)
CFLAGS += -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -O2
CFLAGS += -funwind-tables -fno-omit-frame-pointer
else ifeq ($(PLATFORM), riscv)
CFLAGS += -mcpu=rv32imac -march=rv32imac -mabi=ilp32 -O2
else ifeq ($(PLATFORM), esp32)
CFLAGS += -O2
else ifeq ($(PLATFORM), posix)
CFLAGS += -g -O0
endif

# Feature flags
CFLAGS += -DMICROPOSIX_SCHED_PREEMPTIVE=1
CFLAGS += -DMICROPOSIX_CPU_PROFILE=1
CFLAGS += -DMICROPOSIX_LEAK_DETECT=1
CFLAGS += -DMICROPOSIX_SHELL_ENABLE=1
CFLAGS += -DMICROPOSIX_BLE_ENABLE=1

# Include paths
INCLUDES = -Iinclude
INCLUDES += -Iinclude/microposix
INCLUDES += -Iinclude/microposix/kernel
INCLUDES += -Iinclude/microposix/mm
INCLUDES += -Iinclude/microposix/debug
INCLUDES += -Iinclude/microposix/hal
INCLUDES += -Iinclude/microposix/ble
INCLUDES += -Iinclude/microposix/bootloader

# Platform-specific includes
ifeq ($(PLATFORM), esp32)
INCLUDES += -Iinclude/microposix/hal/esp32
else ifeq ($(PLATFORM), arm)
INCLUDES += -Iinclude/microposix/hal/arm
else ifeq ($(PLATFORM), riscv)
INCLUDES += -Iinclude/microposix/hal/riscv
endif

CFLAGS += $(INCLUDES)

# Source files
SRC_KERNEL = src/kernel/scheduler.c \
             src/kernel/thread.c \
             src/kernel/ipc.c \
             src/kernel/timer.c \
             src/kernel/mq.c \
             src/kernel/abi.c \
             src/kernel/watchdog.c

SRC_MM = src/mm/tlsf.c \
         src/mm/pool.c

SRC_DEBUG = src/debug/log.c \
            src/debug/shell.c \
            src/debug/fault.c

SRC_BLE = src/ble/ble_mgr.c

SRC_BOOT = src/bootloader/boot.c

# Platform-specific sources
ifeq ($(PLATFORM), esp32)
SRC_PLATFORM = platform/esp32/context_switch.c \
               platform/esp32/cpu.c \
               platform/esp32/gpio.c \
               platform/esp32/uart.c \
               platform/esp32/wdt.c \
               platform/esp32/mpu.c \
               src/kernel/abi_esp32.c \
               src/ble/ble_esp32.c
else ifeq ($(PLATFORM), arm)
SRC_PLATFORM = platform/arm/cortex-m4/context_switch.c \
               platform/arm/cortex-m4/mpu.c
else ifeq ($(PLATFORM), riscv)
SRC_PLATFORM = platform/riscv/rv32imac/context_switch.c
else ifeq ($(PLATFORM), posix)
SRC_PLATFORM = platform/posix/context_switch.c
endif

# Object files
OBJ_KERNEL = $(SRC_KERNEL:.c=.o)
OBJ_MM = $(SRC_MM:.c=.o)
OBJ_DEBUG = $(SRC_DEBUG:.c=.o)
OBJ_BLE = $(SRC_BLE:.c=.o)
OBJ_BOOT = $(SRC_BOOT:.c=.o)
OBJ_PLATFORM = $(SRC_PLATFORM:.c=.o)

# Targets
TARGET_LIB = libmicroposix.a
TARGET_ELF = microposix.elf
TARGET_BIN = microposix.bin
TARGET_HEX = microposix.hex

# Default target
all: $(TARGET_LIB)

# Build the library
$(TARGET_LIB): $(OBJ_KERNEL) $(OBJ_MM) $(OBJ_DEBUG) $(OBJ_BLE) $(OBJ_BOOT) $(OBJ_PLATFORM)
	$(AR) rcs $@ $^

# Build the ELF file (for testing)
$(TARGET_ELF): $(OBJ_KERNEL) $(OBJ_MM) $(OBJ_DEBUG) $(OBJ_BLE) $(OBJ_BOOT) $(OBJ_PLATFORM)
	$(CC) $(CFLAGS) -o $@ $^ -nostdlib -Wl,-T,platform/$(PLATFORM)/kernel.ld

# Build the BIN file
$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

# Build the HEX file
$(TARGET_HEX): $(TARGET_ELF)
	$(OBJCOPY) -O ihex $< $@

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(OBJ_KERNEL) $(OBJ_MM) $(OBJ_DEBUG) $(OBJ_BLE) $(OBJ_BOOT) $(OBJ_PLATFORM)
	rm -f $(TARGET_LIB) $(TARGET_ELF) $(TARGET_BIN) $(TARGET_HEX)

# ESP32-specific targets
ifeq ($(PLATFORM), esp32)

# Build with ESP-IDF
idf: 
	@echo "Building with ESP-IDF..."
	@cd demo/esp32 && idf.py build

# Flash to ESP32
flash: idf
	@echo "Flashing to ESP32..."
	@cd demo/esp32 && idf.py -p $(PORT) flash

# Monitor ESP32
monitor: 
	@echo "Monitoring ESP32..."
	@cd demo/esp32 && idf.py -p $(PORT) monitor

# Full build and flash
build-flash: idf flash

endif

# Test target
test: $(TARGET_LIB)
	@echo "Running tests..."
	cd tests && make

# Demo targets
demo: $(TARGET_LIB)
	@echo "Building demo..."
	cd demo/esp32 && make

# Install target
install: $(TARGET_LIB)
	@echo "Installing library..."
	mkdir -p $(DESTDIR)/lib
	cp $(TARGET_LIB) $(DESTDIR)/lib/
	mkdir -p $(DESTDIR)/include/microposix
	cp -r include/microposix/* $(DESTDIR)/include/microposix/

# Size target
size: $(TARGET_ELF)
	@echo "Size report:"
	$(SIZE) -A -d $<

# Disassembly target
disasm: $(TARGET_ELF)
	$(OBJDUMP) -d $< > microposix.disasm

.PHONY: all clean test demo install size disasm idf flash monitor build-flash
