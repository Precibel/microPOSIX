# microPOSIX User Manual: Developing for TI CC2340 (Cortex-M0+)

This guide provides a comprehensive walkthrough for developing applications on the **Texas Instruments CC2340** using the **microPOSIX Resident Kernel**.

---

## 1. Architectural Overview

On the CC2340 (Minimal Profile), microPOSIX operates as a **Resident Kernel**. This means the OS, BLE stack, and drivers are already present on the device. Your application is a separate binary that interacts with the OS via a **System Jump Table ABI**.

- **OS Location**: `0x0000_4000` to `0x0003_FDFF`
- **Jump Table Location**: `0x0003_FE00` (Fixed Address)
- **App Slot 0**: `0x0004_4000` (Start of Application Flash)
- **App RAM**: `0x2003_0000` (Start of Application RAM)

---

## 2. Prerequisites

### Toolchain
- **ARM GNU Toolchain**: `arm-none-eabi-gcc` (v12.x or higher recommended)
- **Build System**: `make` or `cmake`
- **Flashing Tool**: UniFlash or J-Link Commander

### System Library
You will need the `libmicroposix_abi.a` and the associated headers to compile your application. These provide the wrappers that route your C calls to the Resident OS.

---

## 3. Creating Your First Application

### Step 1: Define the Application Header
Every microPOSIX application must start with a header that tells the Resident Kernel how to launch it.

```c
#include <stdint.h>
#include "microposix/kernel/abi.h"

void app_main(void); // Your entry point

typedef struct {
    uint32_t magic;           // Must be 0xAPP0BEEF
    uint32_t version;         // Your app version
    void (*app_entry)(void);  // Pointer to your entry point
} mp_app_header_t;

// Place this at the very beginning of your flash (0x00044000)
__attribute__((section(".app_header"), used))
const mp_app_header_t app_hdr = {
    .magic = 0xAPP0BEEF,
    .version = 0x010000,
    .app_entry = app_main
};
```

### Step 2: Write Your Application Logic
Use standard POSIX calls. The ABI will handle the redirection.

```c
#include "pthread.h"
#include "microposix/debug/log.h"

void app_main(void) {
    // Your initialization code
    printf("Hello from CC2340 Application!\n");

    while(1) {
        // Your business logic
        mp_thread_sleep(1000); 
    }
}
```

---

## 4. Compiling and Linking

### Linker Script Requirements
Your application must use a specific linker script to ensure it doesn't overlap with the Resident OS.

**Key Sections in `app.ld`:**
```linker
MEMORY
{
    FLASH (rx) : ORIGIN = 0x00044000, LENGTH = 128K
    RAM (rwx)  : ORIGIN = 0x20030000, LENGTH = 36K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.app_header)) /* Ensure header is at 0x00044000 */
        *(.text*)
    } > FLASH
}
```

### Build Command
```bash
arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -T app.ld -Iinclude \
    main.c -o app.elf -Llib -lmicroposix_abi
```

---

## 5. Deployment

1. **Flash the Resident OS**: (If not already present) Flash the `microposix_kernel_cc2340.bin` to `0x0000_4000`.
2. **Flash Your App**: Flash your compiled `app.bin` to `0x0004_4000`.
3. **Reboot**: The OS will boot, verify the magic number at `0x0004_4000`, and launch your `app_main`.

---

## 6. Debugging with the Resident Shell

Connect your serial terminal to **UART0** at **921600 baud**. Use the following commands to monitor your app:

- `top`: See your application thread's CPU usage and stack health.
- `mem`: Monitor your application's heap allocations.
- `uptime`: Verify the system is running stably.

---

## 7. Best Practices

- **Stack Size**: Keep stacks lean (e.g., 512B) as the CC2340 has limited RAM.
- **Interrupts**: Do not attempt to register hardware interrupts directly. Use the microPOSIX event system to handle asynchronous events.
- **FOTA**: When updating, always upload your new binary to **Slot 1** (`0x0006_4000`). The Resident OS will handle the swap and verification.

---
*microPOSIX User Manual v1.0 | Target: TI CC2340*
