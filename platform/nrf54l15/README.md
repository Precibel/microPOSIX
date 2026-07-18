# nRF54L15 Platform Support for microPOSIX

This directory contains the platform-specific code for the **Nordic nRF54L15** SoC, which features a **dual-core architecture** with:
- **Cortex-M33** (Arm) core
- **RISC-V** (RV32IMAC) core

## Architecture Overview

The nRF54L15 support implements **Asymmetric Multi-Processing (AMP)** where:
- Each core runs its own microPOSIX instance
- Cores communicate via **Inter-Core Communication (IPC)** mechanisms
- Shared memory is used for synchronization and data exchange

### Key Features:
- **Dual-core support** (M33 + RV32)
- **IPC layer** (mailbox, semaphores, events)
- **Shared memory** for thread registry and synchronization primitives
- **Core-specific context switching** (PendSV for M33, software interrupt for RV32)
- **Cross-core mutexes** for synchronization
- **Core synchronization** (barriers, rendezvous, handshake)

## Directory Structure

```
platform/nrf54l15/
├── CMakeLists.txt          # Platform-specific CMake configuration
├── README.md               # This file
├── m33/                   # Cortex-M33 core files
│   ├── context_switch.c    # Context switching implementation
│   ├── cpu.c               # CPU-specific functions
│   ├── startup.c           # Startup code
│   ├── irq_handlers.c     # Interrupt handlers
│   └── linker.ld           # Linker script
├── rv32/                  # RISC-V core files
│   ├── context_switch.c    # Context switching implementation
│   ├── cpu.c               # CPU-specific functions
│   ├── startup.c           # Startup code
│   ├── irq_handlers.c     # Interrupt handlers
│   └── linker.ld           # Linker script
└── shared/                # Shared between cores
    ├── ipc.c               # IPC implementation
    ├── ipc.h               # IPC header
    ├── shared_memory.c     # Shared memory implementation
    ├── shared_memory.h     # Shared memory header
    └── core_sync.c         # Core synchronization primitives

include/microposix/hal/nrf54l15/
├── shared/                # Shared headers
│   ├── ipc.h
│   ├── shared_memory.h
│   └── core_sync.h
├── m33/                   # M33-specific headers
└── rv32/                  # RV32-specific headers
```

## Building for nRF54L15

### Prerequisites

1. **Toolchains**:
   - Arm: `arm-none-eabi-gcc` (for Cortex-M33)
   - RISC-V: `riscv32-none-elf-gcc` (for RV32)

2. **CMake** (version 3.5 or later)

3. **nRF54L15 SDK** (optional, for hardware-specific headers)

### Build Commands

#### For Cortex-M33 Core:
```bash
mkdir build_m33 && cd build_m33
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/nrf54l15-arm.cmake \
      -DMICROPOSIX_PLATFORM=nrf54l15 \
      -DMICROPOSIX_CORE_M33=1 \
      ..
make
```

#### For RISC-V Core:
```bash
mkdir build_rv32 && cd build_rv32
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/nrf54l15-riscv.cmake \
      -DMICROPOSIX_PLATFORM=nrf54l15 \
      -DMICROPOSIX_CORE_RV32=1 \
      ..
make
```

### Output Files

Each build will produce:
- `microposix.lib` - The microPOSIX library for that core
- `microposix.elf` - The executable (if building an application)

## IPC Layer

The **Inter-Core Communication (IPC)** layer provides:

### Mailbox
- **Blocking send/receive**: `mp_hal_nrf54l15_ipc_send_blocking()` / `mp_hal_nrf54l15_ipc_receive_blocking()`
- **Non-blocking send/receive**: `mp_hal_nrf54l15_ipc_send_nonblocking()` / `mp_hal_nrf54l15_ipc_receive_nonblocking()`
- **Message types**: Defined in `ipc.h` (e.g., `IPC_MSG_TYPE_CORE_READY`, `IPC_MSG_TYPE_THREAD_CREATE`)

### Semaphores
- **Give**: `mp_hal_nrf54l15_ipc_semaphore_give()`
- **Take (blocking)**: `mp_hal_nrf54l15_ipc_semaphore_take()`
- **Try take (non-blocking)**: `mp_hal_nrf54l15_ipc_semaphore_try_take()`

### Events
- **Set**: `mp_hal_nrf54l15_ipc_event_set()`
- **Clear**: `mp_hal_nrf54l15_ipc_event_clear()`
- **Wait**: `mp_hal_nrf54l15_ipc_event_wait()`
- **Check**: `mp_hal_nrf54l15_ipc_event_check()`

### Core Management
- **Start other core**: `mp_hal_nrf54l15_start_other_core()`
- **Get core ID**: `mp_hal_nrf54l15_get_core_id()`
- **Get other core ID**: `mp_hal_nrf54l15_get_other_core_id()`
- **Signal ready**: `mp_hal_nrf54l15_signal_core_ready()`
- **Wait for other core**: `mp_hal_nrf54l15_wait_for_other_core_ready()`

## Shared Memory

The **shared memory** region (64KB on nRF54L15) contains:

### Thread Registry
- **Register thread**: `mp_hal_nrf54l15_register_shared_thread()`
- **Unregister thread**: `mp_hal_nrf54l15_unregister_shared_thread()`
- **Lookup thread**: `mp_hal_nrf54l15_lookup_shared_thread()`

### Synchronization Primitives
- **Spinlocks**: `mp_hal_nrf54l15_spinlock_acquire()` / `mp_hal_nrf54l15_spinlock_release()`
- **Cross-core mutexes**: `mp_hal_nrf54l15_cross_core_mutex_lock()` / `mp_hal_nrf54l15_cross_core_mutex_unlock()`

### Core Synchronization
- **Barrier**: `mp_hal_nrf54l15_core_barrier_wait()` / `mp_hal_nrf54l15_core_barrier_release()`
- **Rendezvous**: `mp_hal_nrf54l15_core_rendezvous()`
- **Handshake**: `mp_hal_nrf54l15_core_handshake()`

## Context Switching

### Cortex-M33 (M33)
- Uses **PendSV interrupt** for context switching
- Stack frame includes: R0-R12, LR, PC, xPSR
- FPU support (lazy stacking)

### RISC-V (RV32)
- Uses **machine-mode software interrupt** for context switching
- Stack frame includes: x1 (RA), x3-x31, PC
- No FPU (RV32IMAC is integer-only)

## Boot Process

1. **M33 Core Boots First**:
   - Initializes shared memory
   - Initializes IPC
   - Starts the RV32 core via `CORESTART` register
   - Waits for RV32 to signal readiness
   - Signals its own readiness

2. **RV32 Core Boots**:
   - Initializes its own stack and data sections
   - Initializes IPC
   - Signals its readiness to M33
   - Waits for M33 to signal readiness (optional)

3. **Both Cores Ready**:
   - Each core initializes its own scheduler
   - Threads can be created on either core
   - Cross-core communication via IPC

## Memory Layout

### Cortex-M33 (M33)
- **Flash**: 0x00000000 - 0x001FFFFF (2MB)
- **SRAM**: 0x20000000 - 0x2003FFFF (256KB)
- **Shared SRAM**: 0x20040000 - 0x2004FFFF (64KB)

### RISC-V (RV32)
- **Flash**: 0x01000000 - 0x011FFFFF (2MB)
- **SRAM**: 0x20080000 - 0x200BFFFF (256KB)
- **Shared SRAM**: 0x20040000 - 0x2004FFFF (64KB)

## Example Usage

### Creating a Thread on the Other Core

```c
#include "microposix/hal/nrf54l15/shared/ipc.h"

void thread_function(void *arg) {
    while (1) {
        // Thread code
    }
}

void create_thread_on_other_core(void) {
    ipc_message_t msg;
    mp_hal_nrf54l15_ipc_init_message(&msg, IPC_MSG_TYPE_THREAD_CREATE);
    msg.param1 = (uint32_t)thread_function;
    msg.param2 = (uint32_t)NULL;  // Argument
    msg.param3 = 5;  // Priority
    
    mp_hal_nrf54l15_ipc_send_blocking(&msg);
}
```

### Using a Cross-Core Mutex

```c
#include "microposix/hal/nrf54l15/shared/shared_memory.h"

shared_mutex_t *mutex = mp_hal_nrf54l15_get_cross_core_mutex(0);

void critical_section(void) {
    mp_hal_nrf54l15_cross_core_mutex_lock(mutex, 1000);
    
    // Critical section code
    
    mp_hal_nrf54l15_cross_core_mutex_unlock(mutex);
}
```

### Synchronizing Cores with a Barrier

```c
#include "microposix/hal/nrf54l15/shared/core_sync.h"

void synchronize_cores(void) {
    // Both cores call this function
    mp_hal_nrf54l15_core_barrier_wait(1000000);  // 1M cycles timeout
    
    // All cores have reached this point
    
    if (mp_hal_nrf54l15_get_core_id() == CORE_ID_M33) {
        // M33-specific code
        mp_hal_nrf54l15_core_barrier_release();
    }
}
```

## Configuration Options

The nRF54L15 platform supports the following CMake options:

- `MICROPOSIX_NRF54L15_AMP`: Enable Asymmetric Multi-Processing (default: ON)
- `MICROPOSIX_NRF54L15_SMP`: Enable Symmetric Multi-Processing (default: OFF)
- `MICROPOSIX_CORE_M33`: Build for Cortex-M33 core
- `MICROPOSIX_CORE_RV32`: Build for RISC-V core

## Limitations

1. **No Cache Coherence**: The nRF54L15 does not have hardware cache coherence between cores. Software must ensure proper synchronization.

2. **Shared Memory Alignment**: All shared data structures must be properly aligned for both Arm and RISC-V access.

3. **IPC Latency**: Inter-core communication has latency. For real-time applications, minimize cross-core synchronization.

4. **Thread Migration**: Threads cannot migrate between cores in AMP mode. Each thread is bound to a specific core.

## Testing

### On QEMU (if available)
```bash
qemu-system-arm -machine nrf54l15 -kernel m33_output.elf -serial mon:stdio
qemu-system-riscv32 -machine nrf54l15 -kernel rv32_output.elf -serial mon:stdio
```

### On Hardware
1. Flash the M33 binary to the M33 core address
2. Flash the RV32 binary to the RV32 core address
3. Reset the device

## Future Enhancements

- [ ] **SMP Support**: Implement Symmetric Multi-Processing with a shared scheduler
- [ ] **DMA for IPC**: Use DMA for faster inter-core data transfer
- [ ] **Hardware Spinlocks**: Use nRF54L15 hardware spinlocks instead of software implementations
- [ ] **Power Management**: Add support for core-specific power modes
- [ ] **Debugging**: Add core-specific debugging via SWO or JTAG
- [ ] **Performance Monitoring**: Add cross-core performance counters

## References

- [nRF54L15 Product Specification](https://www.nordicsemi.com/Products/nRF54L15)
- [nRF54L15 Reference Manual](https://www.nordicsemi.com/Products/nRF54L15)
- [Cortex-M33 Technical Reference Manual](https://developer.arm.com/documentation/100237)
- [RISC-V Specification](https://riscv.org/specifications/)

## License

This code is part of the microPOSIX project and is licensed under the same terms as the main project.
