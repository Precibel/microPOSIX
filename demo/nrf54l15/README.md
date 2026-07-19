# nRF54L15 Demo Application

This directory contains a **demo application** for the Nordic nRF54L15 dual-core SoC that demonstrates the microPOSIX dual-core support.

## Overview

The demo application showcases:
1. **Inter-Core Communication (IPC)** between Cortex-M33 and RISC-V cores
2. **Thread management** on both cores
3. **Synchronization primitives** (mutexes, spinlocks, barriers, rendezvous)
4. **Shared memory** access for global state

## Demo Features

### 1. IPC Test (`ipc_test.c`)
- **Mailbox**: Blocking and non-blocking message passing
- **Semaphores**: 4 hardware semaphores for synchronization
- **Events**: Event flags for signaling between cores
- **Core Management**: Starting cores, signaling readiness, waiting for other core

### 2. Thread Test (`thread_test.c`)
- **Thread Creation**: Creating threads on each core
- **Thread Suspension/Resumption**: Suspending and resuming threads
- **Thread Priority**: Testing priority-based scheduling
- **Shared Thread Registry**: Registering threads in the global registry

### 3. Synchronization Test (`sync_test.c`)
- **Spinlocks**: Test acquire/release with timeout
- **Cross-Core Mutexes**: Test lock/unlock with recursion support
- **Barriers**: Synchronize all cores at a specific point
- **Rendezvous**: Exchange data between cores
- **Handshake**: Simple core-to-core signaling

### 4. Main Application (`main.c`)
- Initializes both cores
- Creates threads on each core
- Runs all demo tests
- Demonstrates cross-core communication

## Building the Demo

### Prerequisites

1. **Toolchains**:
   - Arm: `arm-none-eabi-gcc` (for Cortex-M33)
   - RISC-V: `riscv32-none-elf-gcc` (for RV32)

2. **CMake** (version 3.5 or later)

### Build for Cortex-M33 Core

```bash
# Create build directory
mkdir build_m33 && cd build_m33

# Configure with Arm toolchain
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/nrf54l15-arm.cmake \
      -DMICROPOSIX_PLATFORM=nrf54l15 \
      -DMICROPOSIX_CORE_M33=1 \
      -DDEMO_M33=ON \
      -DDEMO_RV32=OFF \
      ..

# Build the demo
make nrf54l15_m33_demo
```

### Build for RISC-V Core

```bash
# Create build directory
mkdir build_rv32 && cd build_rv32

# Configure with RISC-V toolchain
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/nrf54l15-riscv.cmake \
      -DMICROPOSIX_PLATFORM=nrf54l15 \
      -DMICROPOSIX_CORE_RV32=1 \
      -DDEMO_M33=OFF \
      -DDEMO_RV32=ON \
      ..

# Build the demo
make nrf54l15_rv32_demo
```

### Build Both Cores (Separate Directories)

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

## Output Files

Each build will produce:
- `nrf54l15_m33_demo.elf` - M33 executable
- `nrf54l15_rv32_demo.elf` - RV32 executable
- `libmicroposix.a` - microPOSIX library

## Running the Demo

### On Hardware (nRF54L15 DK)

1. **Flash the M33 binary** to the M33 core address:
   ```bash
   nrfjprog --program nrf54l15_m33_demo.hex --sectorerase -f NRF54
   ```

2. **Flash the RV32 binary** to the RV32 core address:
   ```bash
   # Note: Actual command depends on Nordic's tools for nRF54L15
   ```

3. **Reset the device**

4. **Observe behavior**:
   - M33 core boots first, initializes shared memory
   - M33 starts RV32 core
   - Both cores initialize and signal readiness
   - Threads are created on both cores
   - IPC messages are exchanged between cores
   - Synchronization tests run

### On QEMU (if available)

```bash
# Run M33 core
qemu-system-arm -machine nrf54l15 -kernel nrf54l15_m33_demo.elf -serial mon:stdio

# In another terminal, run RV32 core
qemu-system-riscv32 -machine nrf54l15 -kernel nrf54l15_rv32_demo.elf -serial mon:stdio
```

## Demo Flow

1. **Boot Process**:
   - M33 core boots first
   - Initializes shared memory
   - Starts RV32 core via `CORESTART` register
   - Waits for RV32 to signal readiness
   - Signals its own readiness

2. **Thread Creation**:
   - M33 creates 2 threads (`M33_Thread_1`, `M33_Thread_2`)
   - RV32 creates 2 threads (`RV32_Thread_1`, `RV32_Thread_2`)
   - Threads are registered in the shared thread registry

3. **IPC Communication**:
   - M33 threads send messages to RV32
   - RV32 threads receive and process messages
   - Messages are echoed back or counted

4. **Synchronization**:
   - Cores synchronize using barriers
   - Rendezvous is used to exchange data
   - Handshake ensures both cores are ready

5. **Continuous Operation**:
   - Threads run continuously
   - Counters are incremented
   - IPC messages are exchanged periodically

## Customizing the Demo

### Enabling/Disabling Features

The demo has three main test features that can be enabled/disabled:

- **IPC Test**: `DEMO_IPC_TEST` (default: ON)
- **Thread Test**: `DEMO_THREAD_TEST` (default: ON)
- **Sync Test**: `DEMO_SYNC_TEST` (default: ON)

To disable a feature:
```bash
cmake -DDEMO_IPC_TEST=OFF ..
```

### Adding Custom Tests

1. Create a new test file in `demo/nrf54l15/`
2. Add the test function prototype to a header
3. Call the test function from `main.c`
4. Add the file to `demo/nrf54l15/CMakeLists.txt`

Example:
```c
// In your test file
void my_custom_test(void) {
    // Your test code here
}

// In main.c
my_custom_test();
```

### Modifying Thread Behavior

Edit the thread functions in `main.c`:
- `m33_thread_1()` - M33 core, thread 1
- `m33_thread_2()` - M33 core, thread 2
- `rv32_thread_1()` - RV32 core, thread 1
- `rv32_thread_2()` - RV32 core, thread 2

## Expected Output

When running on hardware or QEMU, you should observe:

1. **M33 Core**:
   - Initializes first
   - Starts RV32 core
   - Creates threads
   - Sends IPC messages to RV32
   - Receives responses from RV32

2. **RV32 Core**:
   - Initializes after being started by M33
   - Signals readiness
   - Creates threads
   - Receives IPC messages from M33
   - Sends responses back to M33

3. **Both Cores**:
   - Threads run continuously
   - Counters increment
   - Synchronization tests pass
   - IPC messages are exchanged

## Debugging

### UART Output

If UART is configured, you can observe debug output:
```bash
# Connect to UART (adjust port as needed)
screen /dev/ttyACM0 115200
```

### JTAG Debugging

Use OpenOCD or J-Link to debug:
```bash
# For M33 core
openocd -f interface/jlink.cfg -f target/nrf54l15.cfg

# For RV32 core
# (May require separate configuration)
```

### Logging

The demo uses simple counters to track activity. For more detailed logging:
1. Add UART support to the platform
2. Use `mp_log()` functions from the debug module
3. Enable `MICROPOSIX_SHELL_ENABLE` for interactive debugging

## Troubleshooting

### Common Issues

1. **RV32 core doesn't start**:
   - Check that M33 is calling `mp_hal_nrf54l15_start_other_core()`
   - Verify the `CORESTART` register address
   - Ensure RV32 binary is flashed to the correct address

2. **IPC messages not received**:
   - Check that both cores have initialized IPC with `mp_hal_nrf54l15_ipc_init()`
   - Verify that interrupts are enabled
   - Check that the mailbox registers are accessible

3. **Synchronization tests hang**:
   - Ensure both cores are running
   - Check that the shared memory is properly initialized
   - Verify that spinlocks are being released

4. **Build errors**:
   - Ensure the correct toolchain is installed
   - Check that CMake can find the toolchain files
   - Verify that the linker scripts are correct for your memory layout

### Checking Core Status

Add this to your code to check core status:
```c
uint32_t core_id = mp_hal_nrf54l15_get_core_id();
shared_memory_t *shared = mp_hal_nrf54l15_get_shared_memory();

// Check if other core is ready
bool other_ready = (shared->core_ready_flags & (1 << mp_hal_nrf54l15_get_other_core_id())) != 0;
```

## Performance Considerations

1. **IPC Latency**: Inter-core communication has latency. For performance-critical applications:
   - Minimize cross-core communication
   - Use shared memory for data that doesn't change often
   - Batch messages when possible

2. **Spinlocks**: Spinlocks can waste CPU cycles. Use them for short critical sections only.

3. **Thread Priorities**: Set appropriate priorities for threads based on their importance.

4. **Stack Sizes**: Ensure thread stacks are large enough for their functions.

## Memory Usage

The demo uses the following memory:

### M33 Core
- **Flash**: ~10-20KB (depending on build options)
- **SRAM**: ~5-10KB (stacks, heap, data)
- **Shared SRAM**: ~1KB (IPC, thread registry, synchronization)

### RV32 Core
- **Flash**: ~10-20KB (depending on build options)
- **SRAM**: ~5-10KB (stacks, heap, data)
- **Shared SRAM**: ~1KB (shared with M33)

## Future Enhancements

- [ ] Add UART output for better debugging
- [ ] Add LED blinking for visual feedback
- [ ] Add button input for interactive control
- [ ] Add performance monitoring (CPU usage, IPC latency)
- [ ] Add power management tests
- [ ] Add BLE functionality (if available on nRF54L15)

## References

- [nRF54L15 Product Specification](https://www.nordicsemi.com/Products/nRF54L15)
- [microPOSIX nRF54L15 Platform Support](../platform/nrf54l15/README.md)
- [microPOSIX Documentation](../../microPOSIX_Final_Architecture_Document.md)

## License

This demo application is part of the microPOSIX project and is licensed under the same terms as the main project.
