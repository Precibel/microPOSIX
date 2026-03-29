# microPOSIX Resident Kernel — Architectural Specification

microPOSIX is a deterministic, preemptive **Resident Kernel RTOS** designed for high-reliability BLE applications. It bridges the gap between traditional monolithic firmware and desktop-class operating systems by implementing a formal **Application Binary Interface (ABI)** and hardware-enforced sandboxing.

---

## 🏗️ System Architecture

The microPOSIX architecture decouples the core OS and BLE stack from the user application, allowing the application to be updated independently via FOTA without jeopardizing the system's core stability.

```mermaid
flowchart TD 
     %% Styling Definitions 
     classDef os fill:#1f77b4,stroke:#333,stroke-width:2px,color:#fff; 
     classDef app fill:#ff7f0e,stroke:#333,stroke-width:2px,color:#fff; 
     classDef mem fill:#2ca02c,stroke:#333,stroke-width:2px,color:#fff; 
     classDef sys fill:#d62728,stroke:#333,stroke-width:2px,color:#fff; 
     classDef tme fill:#9467bd,stroke:#333,stroke-width:2px,color:#fff; 
 
     subgraph Flash [1. Flash Memory Execute-In-Place] 
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
 
     subgraph RAM [2. RAM Partitioning & Sandboxing] 
         direction TB 
         OS_RAM[OS Private RAM\nHeap, BLE Buffers]:::os 
         MPU{Hardware MPU\nBoundary}:::sys 
         App_RAM[Application RAM\nData, BSS, App Stacks]:::app 
 
         OS_RAM --- MPU 
         MPU --- App_RAM 
     end 
 
     subgraph Runtime [3. Execution & ABI Routing] 
         direction LR 
         App_Thread([App Thread\nUnprivileged]):::app 
         Syscall{ABI Call\nSVC or Jump}:::sys 
         OS_Kernel([microPOSIX Kernel\nPrivileged]):::os 
 
         App_Thread -->|Calls nimble_malloc| Syscall 
         Syscall -->|Traps to OS| OS_Kernel 
         OS_Kernel -->|Allocates Memory| App_RAM 
     end 
 
     subgraph Tracking [4. Thread Management Engine - TME] 
         direction TB 
         TCB[Thread Control Block]:::tme 
         StackWalker[Stack Canary Walker]:::tme 
         LeakTracker[Heap Leak List]:::tme 
         UART[UART0 Diagnostics CLI]:::os 
 
         OS_Kernel -->|Updates Runtime Stats| TCB 
         StackWalker -.->|Scans for 0xDEADBEEF| TCB 
         LeakTracker -.->|Logs Allocation PC| TCB 
         TCB -->|Exposes Data| UART 
     end 
 
     %% Cross-Subsystem Links 
     App0 -.->|XIP Instruction Fetch| App_Thread 
     App_RAM -.->|Stack Memory| TCB 
```

---

## 🚀 Hardware Profiles

### **Advanced Profile: TI CC2755 (Cortex-M33 @ 90MHz)**
- **Privilege Levels**: OS runs in **Privileged Mode**; Application threads run in **Unprivileged Mode**.
- **Security**: Hardware **MPU** configures read/execute/write boundaries for App Code, Data, and Stack.
- **ABI**: **SVC Router** (Supervisor Calls) traps unprivileged app requests into the kernel.
- **Flash**: 1MB total; 448KB for OS, 240KB per App Slot.
- **RAM**: 256KB total; 160KB for OS, 64KB for App.

### **Minimal Profile: TI CC2340R5 (Cortex-M0+ @ 48MHz)**
- **Privilege Levels**: Single-level (Logical Sandboxing).
- **ABI**: **System Jump Table** placed at absolute address `0x0003FE00` for zero-overhead OS calls.
- **Flash**: 512KB total; 224KB for OS, 128KB per App Slot.
- **RAM**: 36KB total; 24KB for OS, 12KB for App.

---

## 🧠 Kernel Features

- **Deterministic Scheduler**: Preemptive priority-based scheduling with 32 levels and priority inheritance.
- **POSIX API**: Standard `pthread`, `sem`, `mq`, and `clock` interfaces for portable application development.
- **Memory Management**: 
  - **TLSF Allocator**: High-performance, O(1) heap management for variable-size objects.
  - **Fixed-Size Pools**: ISR-safe, zero-fragmentation allocation for timing-critical tasks.
  - **Leak Tracking**: Allocation headers include Caller PC and Thread ID for precise leak identification.
- **Thread Management Engine (TME)**:
  - **Stack Watermarking**: Real-time stack health monitoring via `0xAA` pattern scanning.
  - **CPU Profiling**: DWT cycle-counter-based utilization tracking (Advanced Profile).
  - **Watchdog Supervisor**: Per-thread software check-ins linked to hardware WDT.
- **FOTA Engine**: Integrated signature verification and A/B slot swapping for independent application updates.

---

## 📟 Resident OS Shell (UART0)

The shell provides a real-time terminal on UART0 (921600 baud) for diagnostics and control.

| Command | Description |
| :--- | :--- |
| `help` | List all available shell commands. |
| `top` | Show real-time CPU utilization, state, and Stack HWM per thread. |
| `mem` | Report heap usage, free space, and fragmentation metrics. |
| `uptime` | System uptime from the 64-bit monotonic clock. |
| `kill <tid>` | Safely terminate an application thread. |
| `ble stat` | View BLE link statistics, RSSI, and connection intervals. |
| `reboot` | Controlled software reset. |

---

## 📂 Project Structure

- `include/microposix/`: Resident Kernel headers and ABI definitions.
- `src/kernel/`: Core scheduler, IPC (mutex, sem, mq), and threading logic.
- `src/mm/`: TLSF and Pool memory allocators.
- `src/debug/`: UART Shell, Log engine, and Fault handlers.
- `src/bootloader/`: Stage-2 loader, Secure UART Update, and FOTA logic.
- `platform/`: Hardware-specific HAL, MPU drivers, and Linker Scripts.
- `src/app_led_blink.c`: Example application using the Resident OS ABI.

---

## 🛠️ Build and Verify

### Prerequisites
- `arm-none-eabi-gcc` v12.x+
- `make`

### Build OS and App
```bash
# Build Resident OS for CC2755
make PROFILE=1

# Build User Application
cd src && arm-none-eabi-gcc -T ../platform/arm/cortex-m4/app.ld app_led_blink.c -o app.elf
```

### Verification
Run the integrated test suite on the POSIX simulation backend:
```bash
cd tests && make
```

---
*microPOSIX v1.0 | Developed by Precibel*
