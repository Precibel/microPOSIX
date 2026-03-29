# microPOSIX Makefile

PROFILE ?= 1
TARGET ?= arm-none-eabi

CC = $(TARGET)-gcc
AR = $(TARGET)-ar
LD = $(TARGET)-ld

INCLUDES = -Iinclude -Iinclude/microposix -Iinclude/microposix/kernel -Iinclude/microposix/mm -Iinclude/microposix/debug -Iinclude/microposix/hal -Iinclude/microposix/bootloader

CFLAGS = -Wall -Wextra -Werror -std=c11 -ffunction-sections -fdata-sections $(INCLUDES)

ifeq ($(PROFILE), 1)
CFLAGS += -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -O2 -funwind-tables -fno-omit-frame-pointer -DMICROPOSIX_PROFILE=1
else
CFLAGS += -mcpu=cortex-m0plus -mthumb -Os -DMICROPOSIX_PROFILE=0
endif

# Source files
SRC_KERNEL = $(wildcard src/kernel/*.c)
SRC_MM = $(wildcard src/mm/*.c)
SRC_DEBUG = $(wildcard src/debug/*.c)
SRC_BLE = $(wildcard src/ble/*.c)
SRC_BOOT = $(wildcard src/bootloader/*.c)

OBJ_KERNEL = $(SRC_KERNEL:.c=.o)
OBJ_MM = $(SRC_MM:.c=.o)
OBJ_DEBUG = $(SRC_DEBUG:.c=.o)
OBJ_BLE = $(SRC_BLE:.c=.o)
OBJ_BOOT = $(SRC_BOOT:.c=.o)

all: libmicroposix.a

libmicroposix.a: $(OBJ_KERNEL) $(OBJ_MM) $(OBJ_DEBUG) $(OBJ_BLE) $(OBJ_BOOT)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/kernel/*.o src/mm/*.o src/debug/*.o src/ble/*.o src/bootloader/*.o libmicroposix.a

.PHONY: all clean
