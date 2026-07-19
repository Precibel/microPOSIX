# microPOSIX Resident Kernel 

microPOSIX is a **deterministic, preemptive Resident Kernel RTOS** designed for high-reliability embedded applications. It bridges the gap between traditional monolithic firmware and desktop-class operating systems by implementing a formal **Application Binary Interface (ABI)** and hardware-enforced sandboxing.

---

## Table of Contents
1. [System Architecture](#system-architecture)
2. [Hardware Profiles](#hardware-profiles)
3. [Kernel Features](#kernel-features)
4. [Platform Support](#platform-support)
5. [Resident OS Shell](#resident-os-shell-uart0)
6. [Project Structure](#project-structure)
7. [Demo Applications](#demo-applications)
8. [Build and Verify](#build-and-verify)
9. [Recent Changes](#recent-changes)
10. [Documentation](#documentation)
11. [License](#license)

---

## System Architecture

The microPOSIX architecture decouples the core OS from the user application, allowing the application to be updated independently via FOTA without jeopardizing the system's core stability.

```mermaid
flowchart TD 
     %% Styling Definitions 
     classDef os fill:#1f77b4,stroke:#333,stroke-width:2px,color:#fff; 
     classDef app fill:#ff7f0e,stroke:#333,stroke-width:2px,color:#fff; 
     classDef mem fill:#2ca02c,stroke:#333,stroke-width:2px,color:#fff; 
     classDef sys fill:#d62728,stroke:#333,stroke-width:2px,color:#fff; 
     classDef tme fill:#9467bd,stroke:#333,stroke-width:2px,color:#fff; 
 
     subgraph Flash ["Flash Memory Execute In Place"]

         direction TB 
         Boot[Immutable Bootloader]:::sys 
         OS_Flash[microPOSIX Kernel & BLE]:::os 
         ABI[ABI Interface Table]:::sys 
         NVS[(NVS: Boot Flags)]:::mem 
         App0[App Slot 0: Active Application]:::app 
         App1[App Slot 1: FOTA Staging]:::app 
 
         Boot -->|1. Verifies Signature| OS_Flash 
         OS_Flash -->|2. Reads Active Slot| NVS 
         OS_Flash -->|3. Validates Header| App0 
     end 
 
     subgraph RAM ["2. RAM Partitioning & Sandboxing"]

         direction TB 
         OS_RAM[OS Private RAM\nHeap, BLE Buffers]:::os 
         MPU{Hardware MPU\nBoundary}:::sys 
         App_RAM[Application RAM\nData, BSS, App Stacks]:::app 
 
         OS_RAM --- MPU 
         MPU --- App_RAM 
     end 
 
     subgraph Runtime ["3. Execution & ABI Routing"]

         direction LR 
         App_Thread([App Thread\nUnprivileged]):::app 
         Syscall{ABI Call\nSVC or Jump}:::sys 
         OS_Kernel([microPOSIX Kernel\nPrivileged]):::os 
 
         App_Thread -->|Calls nimble_malloc| Syscall 
         Syscall -->|Traps to OS| OS_Kernel 
         OS_Kernel -->|Allocates Memory| App_RAM 
     end 
 
     subgraph Tracking ["4. Thread Management Engine - TME"]

         direction TB 
         TCB[Thread Control Block]:::tme 
         StackWalker[Stack Canary Walker]:::tme 
         LeakTracker[Heap Leak List]:::tme 
         UART[UART0 Diagnostics CLI]:::os 
 
         OS_Kernel -->|Updates Runtime Stats| TCB 
         StackWalker -.->|Scans for 0xDEADBEEF| TCB 
         LeakTracker -.->|Logs Allocation PC| TCB

     end
```

---

## Hardware Profiles

### Advanced Profile: TI CC2755 (Cortex-M33 @ 90MHz)
- **Privilege Levels**: OS runs in **Privileged Mode**; Application threads run in **Unprivileged Mode**
- **Security**: Hardware **MPU** configures read/execute/write boundaries for App Code, Data, and Stack
- **ABI**: **SVC Router** (Supervisor Calls) traps unprivileged app requests into the kernel
- **Flash**: 1MB total; 448KB for OS, 240KB per App Slot
- **RAM**: 256KB total; 160KB for OS, 64KB for App

### Minimal Profile: TI CC2340R5 (Cortex-M0+ @ 48MHz)
- **Privilege Levels**: Single-level (Logical Sandboxing)
- **ABI**: **System Jump Table** placed at absolute address `0x0003FE00` for zero-overhead OS calls
- **Flash**: 512KB total; 224KB for OS, 128KB per App Slot
- **RAM**: 36KB total; 24KB for OS, 12KB for App

### Nordic nRF54L15 (Dual-Core: Cortex-M33 + RV32)
- **Architecture**: Asymmetric Multi-Processing (AMP) with dual-core support
- **Core 1**: Cortex-M33 with FPU, running independent microPOSIX instance
- **Core 2**: RISC-V RV32IMAC (integer-only), running independent microPOSIX instance
- **IPC**: Hardware mailbox, semaphores, and events for inter-core communication
- **Shared Memory**: 64KB shared SRAM for synchronization primitives and thread registry
- **Memory Layout**: 
  - M33: Flash 0x00000000-0x001FFFFF (2MB), SRAM 0x20000000-0x2003FFFF (256KB)
  - RV32: Flash 0x01000000-0x011FFFFF (2MB), SRAM 0x20080000-0x200BFFFF (256KB)
  - Shared: SRAM 0x20040000-0x2004FFFF (64KB)
- **Features**: Cross-core mutexes, barriers, rendezvous, handshake synchronization
- **Context Switching**: PendSV interrupt for M33, machine-mode software interrupt for RV32

### Espressif ESP32 Family

#### ESP32 (Xtensa LX6)
- **Architecture**: Dual-core Xtensa LX6 @ 80-240MHz
- **Integration**: Uses **FreeRTOS** as the underlying RTOS
- **ABI**: System Jump Table (SVC not available on Xtensa)
- **Memory**: 4MB+ Flash, 520KB+ RAM
- **MPU**: Xtensa MPU with 8 regions
- **Cycle Counter**: Uses `ccount` register
- **Features**: Full thread management, synchronization primitives, GPIO, UART, WDT

#### ESP32-S3 (RISC-V)
- **Architecture**: Dual-core RISC-V @ 80-160MHz
- **Integration**: Uses **FreeRTOS** as the underlying RTOS
- **ABI**: System Jump Table
- **Memory**: 4MB+ Flash, 512KB+ RAM
- **PMP**: RISC-V Physical Memory Protection with 16 regions
- **Cycle Counter**: Uses `mcycle` CSR
- **Features**: Full thread management, synchronization primitives, GPIO, UART, WDT

#### ESP32-C3 (RISC-V)
- **Architecture**: Single-core RISC-V @ 80-160MHz
- **Integration**: Uses **FreeRTOS** as the underlying RTOS
- **ABI**: System Jump Table
- **Memory**: 4MB+ Flash, 384KB+ RAM
- **PMP**: RISC-V Physical Memory Protection with 16 regions
- **Cycle Counter**: Uses `mcycle` CSR
- **Features**: Full thread management, synchronization primitives, GPIO, UART, WDT

---

## Kernel Features

### Core RTOS Features
- **Deterministic Scheduler**: Preemptive priority-based scheduling with 32 levels and priority inheritance
- **POSIX API**: Standard `pthread`, `sem`, `mq`, and `clock` interfaces for portable application development
- **Memory Management**: 
  - **TLSF Allocator**: High-performance, O(1) heap management for variable-size objects
  - **Fixed-Size Pools**: ISR-safe, zero-fragmentation allocation for timing-critical tasks
  - **Leak Tracking**: Allocation headers include Caller PC and Thread ID for precise leak identification
  - **Zero-Copy Memory Views**: Efficient buffer access without copying (memory_view.h)
  - **Shared Heap**: Thread-safe allocation with mutex protection (shared_heap.h)
  - **Garbage Collection**: Reference counting and mark-and-sweep GC (gc.h)
  - **Memory Arenas**: Bulk allocation and zero-copy slicing (memory_view.h)

### Thread Management Engine (TME)
- **Stack Watermarking**: Real-time stack health monitoring via `0xAA` pattern scanning
- **CPU Profiling**: DWT cycle-counter-based utilization tracking (Advanced Profile)
- **Watchdog Supervisor**: Per-thread software check-ins linked to hardware WDT

### Advanced Features
- **FOTA Engine**: Integrated signature verification and A/B slot swapping for independent application updates
- **Inter-Core Communication (IPC)**: Mailbox, semaphores, events for multi-core synchronization (nRF54L15)
- **Cross-Core Synchronization**: Mutexes, barriers, rendezvous, handshake (nRF54L15)
- **Serialization Support**: JSON, Protocol Buffers, EDF encoding/decoding

### Serialization Support
The microPOSIX serialization framework provides:
- **JSON**: Full parser and generator with support for all JSON types
- **Protocol Buffers**: Varint, fixed32/64, length-delimited encoding with zig-zag for signed integers
- **EDF (Extensible Data Format)**: Compact binary serialization with type safety and schema support

---

## Platform Support

### Fully Supported Platforms

| Platform | Architecture | Cores | MPU/PMP | BLE | WiFi | Status |
|----------|-------------|-------|---------|-----|------|--------|
| TI CC2755 | Cortex-M33 | 1 | MPU | BLE5.4 | No | Production |
| TI CC2340R5 | Cortex-M0+ | 1 | MPU | BLE5.4 | No | Production |
| Nordic nRF54L15 | Cortex-M33 + RV32 | 2 | MPU | BLE5.4 | No | Production |
| ESP32 | Xtensa LX6 | 2 | MPU | BLE4.2 | Yes | Production |
| ESP32-S3 | RISC-V | 2 | PMP | BLE5.0 | Yes | Production |
| ESP32-C3 | RISC-V | 1 | PMP | BLE5.0 | Yes | Production |

### Platform-Specific Documentation
- [nRF54L15 Platform Support](platform/nrf54l15/README.md) - Dual-core architecture, IPC, synchronization
- [ESP32 Platform Support](platform/esp32/README.md) - FreeRTOS integration, HAL implementation
- [ESP32 Demo Application](demo/esp32/README.md) - Blinking LED demo with build instructions
- [nRF54L15 Demo Application](demo/nrf54l15/README.md) - Dual-core demo with IPC and synchronization tests
- [ESP32 Compatibility Analysis](demo/esp32/COMPATIBILITY_ANALYSIS.md) - Detailed compatibility assessment
- [ESP32 Demo Summary](demo/esp32/DEMO_SUMMARY.md) - Comprehensive demo overview

---

## Resident OS Shell (UART0)

The shell provides a real-time terminal on UART0 (921600 baud) for diagnostics and control.

| Command | Description |
| :--- | :--- |
| `help` | List all available shell commands |
| `top` | Show real-time CPU utilization, state, and Stack HWM per thread |
| `mem` | Report heap usage, free space, and fragmentation metrics |
| `uptime` | System uptime from the 64-bit monotonic clock |
| `kill <tid>` | Safely terminate an application thread |
| `ble stat` | View BLE link statistics, RSSI, and connection intervals |
| `reboot` | Controlled software reset |

---

## Project Structure

```
microPOSIX/
├── include/microposix/
│   ├── kernel/              # Core kernel headers
│   │   ├── thread.h        # Thread management
│   │   ├── scheduler.h     # Scheduler
│   │   ├── semaphore.h     # Semaphores
│   │   ├── mutex.h         # Mutexes
│   │   ├── message_queue.h # Message queues
│   │   └── ...
│   ├── mm/                 # Memory management
│   │   ├── tlsf.h          # TLSF allocator
│   │   ├── pool.h          # Fixed-size pools
│   │   ├── memory_view.h   # Zero-copy memory views
│   │   ├── shared_heap.h    # Shared heap management
│   │   ├── gc.h            # Garbage collection
│   │   └── memory.h        # Unified memory API
│   ├── hal/                # Hardware Abstraction Layer
│   │   ├── cpu.h           # CPU interface
│   │   ├── arm/            # ARM Cortex-M common
│   │   │   ├── cpu.c
│   │   │   └── context_switch.c
│   │   ├── nrf54l15/       # Nordic nRF54L15
│   │   │   ├── m33/        # Cortex-M33 core
│   │   │   │   ├── context_switch.c
│   │   │   │   ├── cpu.c
│   │   │   │   ├── startup.c
│   │   │   │   ├── irq_handlers.c
│   │   │   │   └── linker.ld
│   │   │   ├── rv32/       # RISC-V core
│   │   │   │   ├── context_switch.c
│   │   │   │   ├── cpu.c
│   │   │   │   ├── startup.c
│   │   │   │   ├── irq_handlers.c
│   │   │   │   └── linker.ld
│   │   │   └── shared/     # Shared between cores
│   │   │       ├── ipc.c
│   │   │       ├── ipc.h
│   │   │       ├── shared_memory.c
│   │   │       ├── shared_memory.h
│   │   │       └── core_sync.c
│   │   └── esp32/          # Espressif ESP32
│   │       ├── cpu.c
│   │       ├── cpu.h
│   │       ├── gpio.c
│   │       ├── gpio.h
│   │       ├── mpu.c
│   │       ├── mpu.h
│   │       ├── uart.c
│   │       ├── uart.h
│   │       ├── wdt.c
│   │       └── wdt.h
│   ├── ble/                # BLE stack interfaces
│   │   ├── ble.h           # BLE API
│   │   ├── ble_esp32.h     # ESP32 BLE backend
│   │   └── ...
│   ├── debug/              # Debug and logging
│   │   ├── log.h           # Logging interface
│   │   ├── shell.h         # Shell interface
│   │   └── ...
│   └── serialization/      # Serialization support
│       ├── json.h          # JSON parser/generator
│       ├── protobuf.h      # Protocol Buffers
│       ├── edf.h           # EDF format
│       └── serialization.h # Unified API
├── src/
│   ├── kernel/             # Core scheduler, IPC, threading
│   │   ├── scheduler.c
│   │   ├── thread.c
│   │   ├── semaphore.c
│   │   ├── mutex.c
│   │   ├── message_queue.c
│   │   └── ...
│   ├── mm/                 # Memory allocators and GC
│   │   ├── tlsf.c
│   │   ├── pool.c
│   │   ├── memory_view.c
│   │   ├── shared_heap.c
│   │   └── gc.c
│   ├── debug/              # UART Shell, Log engine, Fault handlers
│   │   ├── shell.c
│   │   ├── log.c
│   │   └── fault_handlers.c
│   ├── bootloader/         # Stage-2 loader, FOTA logic
│   │   ├── bootloader.c
│   │   ├── fota.c
│   │   └── secure_update.c
│   └── app_led_blink.c     # Example application
├── platform/
│   ├── arm/                # ARM Cortex-M HAL
│   │   ├── cortex-m33/
│   │   └── cortex-m0+
│   ├── nrf54l15/           # nRF54L15 platform
│   │   ├── CMakeLists.txt
│   │   ├── m33/
│   │   ├── rv32/
│   │   └── shared/
│   ├── esp32/              # ESP32 platform
│   │   ├── context_switch.c
│   │   ├── cpu.c
│   │   ├── gpio.c
│   │   ├── mpu.c
│   │   ├── uart.c
│   │   └── wdt.c
│   ├── riscv/              # RISC-V common
│   └── posix/              # POSIX simulation backend
├── demo/
│   ├── esp32/              # ESP32 demo
│   │   ├── CMakeLists.txt
│   │   ├── main.c
│   │   ├── app_blink.c
│   │   ├── README.md
│   │   ├── COMPATIBILITY_ANALYSIS.md
│   │   ├── DEMO_SUMMARY.md
│   │   └── sdkconfig.defaults
│   └── nrf54l15/           # nRF54L15 demo
│       ├── CMakeLists.txt
│       ├── main.c
│       ├── ipc_test.c
│       ├── thread_test.c
│       ├── sync_test.c
│       └── README.md
├── tests/                  # Test suite
│   ├── test_memory.c
│   ├── test_serialization.c
│   └── ...
├── toolchains/             # CMake toolchain files
│   ├── nrf54l15-arm.cmake
│   └── nrf54l15-riscv.cmake
├── docs/                   # Documentation
│   ├── MEMORY_MANAGEMENT.md
│   └── ...
├── CHANGES_SUMMARY.md      # Summary of recent changes
├── IMPLEMENTATION_SUMMARY.txt
├── microPOSIX_Final_Architecture_Document.md
├── USER_MANUAL_CC2340.md
├── README.md               # This file
├── Makefile                # Build system
└── CMakeLists.txt           # CMake build configuration
```

---

## Demo Applications

### ESP32 Demo
The ESP32 demo demonstrates microPOSIX running on ESP32 with FreeRTOS integration.

**Features:**
- microPOSIX kernel initialization on ESP32
- Thread creation and management
- GPIO control using microPOSIX HAL
- Blinking LED application
- Integration with FreeRTOS

**Quick Start:**
```bash
# Prerequisites: Install ESP-IDF v5.0+
cd demo/esp32

# Configure the project
idf.py set-target esp32
idf.py menuconfig

# Build and flash
idf.py build
idf.py -p /dev/ttyUSB0 flash

# Monitor output
idf.py -p /dev/ttyUSB0 monitor
```

**Supported Targets:**
- `esp32` - Standard ESP32 (Xtensa)
- `esp32s3` - ESP32-S3 (RISC-V)
- `esp32c3` - ESP32-C3 (RISC-V)

**Documentation:** [ESP32 Demo README](demo/esp32/README.md)

### nRF54L15 Demo
The nRF54L15 demo demonstrates dual-core functionality with IPC, threading, and synchronization.

**Features:**
- Inter-Core Communication (IPC) between Cortex-M33 and RISC-V cores
- Thread management on both cores
- Synchronization primitives (mutexes, spinlocks, barriers, rendezvous)
- Shared memory access for global state

**Quick Start:**
```bash
# Build M33 core
mkdir build_m33 && cd build_m33
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/nrf54l15-arm.cmake \
      -DMICROPOSIX_PLATFORM=nrf54l15 \
      -DMICROPOSIX_CORE_M33=1 \
      ..
make

# Build RV32 core
cd ..
mkdir build_rv32 && cd build_rv32
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/nrf54l15-riscv.cmake \
      -DMICROPOSIX_PLATFORM=nrf54l15 \
      -DMICROPOSIX_CORE_RV32=1 \
      ..
make
```

**Output Files:**
- `nrf54l15_m33_demo.elf` - M33 executable
- `nrf54l15_rv32_demo.elf` - RV32 executable

**Documentation:** [nRF54L15 Demo README](demo/nrf54l15/README.md)

---

## Build and Verify

### Prerequisites

#### Common Tools
- `arm-none-eabi-gcc` v12.x+ (for ARM Cortex-M)
- `riscv32-none-elf-gcc` (for RISC-V)
- `xtensa-esp32-elf-gcc` (for ESP32 Xtensa)
- `make`
- `cmake` (version 3.5+)

#### Platform-Specific Tools
- **ESP32**: [ESP-IDF v5.0+](https://github.com/espressif/esp-idf)
- **nRF54L15**: Nordic nRF5 SDK (optional, for hardware-specific headers)

### Build Commands

#### For TI CC2755 (Cortex-M33)
```bash
make PROFILE=1
```

#### For TI CC2340R5 (Cortex-M0+)
```bash
make PROFILE=0
```

#### For nRF54L15 (Cortex-M33)
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/nrf54l15-arm.cmake \
      -DMICROPOSIX_PLATFORM=nrf54l15 \
      -DMICROPOSIX_CORE_M33=1 \
      ..
make
```

#### For nRF54L15 (RISC-V)
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/nrf54l15-riscv.cmake \
      -DMICROPOSIX_PLATFORM=nrf54l15 \
      -DMICROPOSIX_CORE_RV32=1 \
      ..
make
```

#### For ESP32
```bash
cd demo/esp32
idf.py set-target esp32
idf.py build
```

### Verification

Run the integrated test suite on the POSIX simulation backend:
```bash
cd tests && make
```

---

## Recent Changes

### New Features Added

#### 1. nRF54L15 Dual-Core Support
- **Full support** for Nordic nRF54L15 SoC with Cortex-M33 and RISC-V cores
- **Asymmetric Multi-Processing (AMP)** architecture
- **Inter-Core Communication (IPC)** layer:
  - Hardware mailbox for message passing
  - 4 hardware semaphores for synchronization
  - Event flags for signaling between cores
- **Shared memory** (64KB) for:
  - Thread registry (global visibility across cores)
  - Synchronization primitives (spinlocks, mutexes)
  - Core synchronization (barriers, rendezvous, handshake)
- **Core-specific context switching**:
  - PendSV interrupt for Cortex-M33
  - Machine-mode software interrupt for RISC-V
- **Boot process**: M33 boots first, initializes shared memory, starts RV32 core

**Files Added:**
- `platform/nrf54l15/` - Complete platform support
- `platform/nrf54l15/m33/` - Cortex-M33 core files
- `platform/nrf54l15/rv32/` - RISC-V core files
- `platform/nrf54l15/shared/` - Shared IPC and synchronization
- `include/microposix/hal/nrf54l15/shared/` - HAL headers
- `toolchains/nrf54l15-arm.cmake` - Arm toolchain
- `toolchains/nrf54l15-riscv.cmake` - RISC-V toolchain
- `demo/nrf54l15/` - Demo application with tests

#### 2. ESP32 Platform Support
- **Full support** for ESP32, ESP32-S3, and ESP32-C3
- **FreeRTOS integration** as the underlying RTOS
- **Architecture support**:
  - Xtensa LX6 (ESP32)
  - RISC-V (ESP32-S3, ESP32-C3)
- **Platform-specific HAL**:
  - CPU initialization and cycle counting
  - MPU (Xtensa) and PMP (RISC-V) memory protection
  - GPIO, UART, WDT drivers
- **Compatibility solutions**:
  - System Jump Table instead of SVC
  - `ccount` (Xtensa) or `mcycle` (RISC-V) instead of DWT
  - Proper privilege level handling

**Files Added:**
- `platform/esp32/` - Complete platform support
- `include/microposix/hal/esp32/` - HAL headers
- `include/microposix/kernel/abi_esp32.h` - ESP32 ABI
- `include/microposix/ble/ble_esp32.h` - ESP32 BLE backend
- `demo/esp32/` - Demo application with blinking LED
- `demo/esp32/COMPATIBILITY_ANALYSIS.md` - Detailed compatibility analysis
- `demo/esp32/DEMO_SUMMARY.md` - Demo summary

#### 3. Advanced Memory Management
- **Zero-Copy Memory Views** (`memory_view.h`):
  - Memory views for efficient buffer access
  - Memory slices for typed access
  - Memory arenas for bulk allocation
  - Overlap detection and safe copying
  - Pattern finding and comparison
- **Shared Heap** (`shared_heap.h`):
  - Thread-safe allocation with mutex protection
  - Allocation tracking with source file and line information
  - Comprehensive statistics (used, free, peak, fragmentation)
  - Defragmentation support
  - Per-thread caching (optional)
- **Garbage Collection** (`gc.h`):
  - Reference counting (deterministic, O(1) overhead)
  - Mark-and-sweep (handles cycles)
  - Generational GC (optimized for embedded)
  - Finalizers for cleanup code
  - Root tracking and object pinning

**Files Added:**
- `include/microposix/mm/memory_view.h`
- `include/microposix/mm/shared_heap.h`
- `include/microposix/mm/gc.h`
- `include/microposix/mm/memory.h`
- `src/mm/memory_view.c`
- `src/mm/shared_heap.c`
- `src/mm/gc.c`

#### 4. Serialization Support
- **JSON** (`json.h`, `json.c`):
  - Full JSON parser and generator
  - Support for all JSON types
  - Pretty printing, custom allocators
- **Protocol Buffers** (`protobuf.h`, `protobuf.c`):
  - Varint, fixed32/64, length-delimited encoding
  - All protobuf wire types
  - Zig-zag encoding for signed integers
- **EDF (Extensible Data Format)** (`edf.h`, `edf.c`):
  - Compact binary serialization
  - Type-safe encoding/decoding
  - Schema support

**Files Added:**
- `include/microposix/serialization/json.h`
- `include/microposix/serialization/protobuf.h`
- `include/microposix/serialization/edf.h`
- `include/microposix/serialization/serialization.h`
- `src/serialization/json.c`
- `src/serialization/protobuf.c`
- `src/serialization/edf.c`

For more details, see:
- [CHANGES_SUMMARY.md](CHANGES_SUMMARY.md) - Comprehensive change summary
- [IMPLEMENTATION_SUMMARY.txt](IMPLEMENTATION_SUMMARY.txt) - Implementation details
- [ESP32 Compatibility Analysis](demo/esp32/COMPATIBILITY_ANALYSIS.md) - ESP32 compatibility assessment
- [ESP32 Demo Summary](demo/esp32/DEMO_SUMMARY.md) - ESP32 demo overview
- [REVIEW_SUMMARY.md](REVIEW_SUMMARY.md) - Code review summary

---

## Documentation

### Architecture and Design
- [microPOSIX Final Architecture Document](microPOSIX_Final_Architecture_Document.md)
- [Memory Management Documentation](docs/MEMORY_MANAGEMENT.md)

### Platform-Specific
- [nRF54L15 Platform Support](platform/nrf54l15/README.md)
- [ESP32 Platform Support](platform/esp32/README.md)

### User Manuals
- [User Manual for CC2340](USER_MANUAL_CC2340.md)

### Demo Applications
- [ESP32 Demo README](demo/esp32/README.md)
- [nRF54L15 Demo README](demo/nrf54l15/README.md)

### Reviews and Analysis
- [REVIEW_SUMMARY.md](REVIEW_SUMMARY.md) - Comprehensive code review
- [ESP32 Compatibility Analysis](demo/esp32/COMPATIBILITY_ANALYSIS.md)
- [ESP32 Demo Summary](demo/esp32/DEMO_SUMMARY.md)

---

## License

This project is licensed under the terms specified in the project files.

---

*microPOSIX v2.0 | Developed by Precibel | Updated with nRF54L15, ESP32, Advanced Memory Management, and Serialization Support*
