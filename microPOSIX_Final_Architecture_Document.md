# microPOSIX — Final Architectural Specification & 90MHz Headroom Analysis
### Consolidated Review, Feature Catalogue, and Engineering Decisions
**Document Status:** Final Draft v1.0 | Target: ARM Cortex-M4F @ 90MHz / Cortex-M0+ @ 20MHz

---

## 1. Executive Summary

microPOSIX is a deterministic, preemptive RTOS providing a curated POSIX-compatible API surface, BLE-optimised scheduling, and an advanced developer observability layer accessible over UART0. The design achieves real-time BLE link stability while delivering features — CPU profiling, per-thread memory accounting, leak detection, watchdog supervision, crash dump — that are typically absent in MCU operating systems of this class.

This document reviews the submitted Architectural Requirements Document (ARD), resolves architectural ambiguities, consolidates all features from both the ARD and the earlier NimbleOS SRS, and provides a quantitative headroom analysis proving that the full feature set consumes approximately **2–3% CPU overhead** on a 90MHz Cortex-M4F, leaving approximately 97% of the processor available for application logic.

---

## 2. ARD Review: Feature-by-Feature Commentary

The submitted ARD is architecturally strong and demonstrates clear understanding of real-time constraints. The following section provides a detailed engineering review of each section.

### 2.1 Preemptive Scheduler — Verdict: Correct, Needs One Refinement

The priority-inversion-free, BLE-controller-at-top-priority model is correct. The **tickless idle** requirement is particularly valuable and well-specified. However, one critical detail must be resolved: the scheduler's sleep duration calculation must integrate with the **BLE controller's timing oracle**. The BLE link layer maintains a precise schedule of future connection events (anchor points). The scheduler must query this oracle before sleeping and constrain the sleep duration to `min(next_os_timer, next_ble_anchor_minus_wakeup_latency)`. On Nordic SoftDevice and Zephyr platforms this is exposed; on custom implementations it must be designed as a first-class interface.

The `pthread_cond_t` requirement deserves careful attention: condition variables perform an **atomic mutex-release + block** operation. On single-core Cortex-M this must be implemented with a brief interrupt-disable window to eliminate the TOCTOU race. The canonical implementation uses the `EXCLUSIVE_ACCESS` instruction pair (`LDREX`/`STREX`) on M4 or critical-section bracketing on M0+.

### 2.2 DWT Cycle Counter for CPU Profiling — Verdict: Correct with Critical Caveat

Using the ARM **Data Watchpoint and Trace (DWT)** unit's cycle counter (`DWT->CYCCNT`) is the correct high-resolution approach on Cortex-M4F. The counter is a 32-bit register clocking at CPU frequency, giving cycle-accurate timing with zero ISR overhead.

**Critical caveat:** The DWT unit is **not present on Cortex-M0+**. The 20MHz profile target (Cortex-M0+, MSP430, EFR32BG22) cannot use this approach. The 20MHz profile must fall back to SysTick-based coarse profiling, which has ~1ms resolution, or disable CPU profiling entirely (recommended). This distinction must be enforced by the configuration system:

```c
#if defined(__ARM_ARCH_7EM__) && defined(MICROPOSIX_CPU_PROFILE)
    // Cortex-M4/M7: DWT cycle counter
    cycles_start = DWT->CYCCNT;
#elif defined(MICROPOSIX_CPU_PROFILE_COARSE)
    // Cortex-M0+: SysTick-based, 1ms resolution only
    ticks_start = SysTick->VAL;
#endif
```

### 2.3 Stack Watermarking (0xAA Pattern) — Verdict: Industry Standard, Enhance

The `0xAA` fill pattern for stack high-water-mark measurement is the standard approach (FreeRTOS uses `0xA5`). The scan is correct. Three enhancements are recommended:

1. **Use `0xDEADBEEF` (or `0xCDCDCDCD`) as the canary word at the stack bottom**, separate from the fill pattern. The canary is checked on every context switch, not just by the background scanner.
2. **The background scan frequency** should be configurable. At 1-second intervals on 90MHz it is trivially cheap; at 10-second intervals on 20MHz it imposes negligible load.
3. **MPU-backed stack guard** (Profile A only): Configure an MPU region of 32 bytes at the stack bottom marked as no-access. A stack overflow will immediately trigger a MemManage fault rather than silently corrupting the heap.

### 2.4 Memory Leak Detection — Verdict: Well-Specified, Enhance PC Capture

The per-allocation header with Thread ID, size, and PC is exactly correct. The implementation detail for capturing PC on ARM requires using the compiler intrinsic:

```c
typedef struct alloc_header {
    struct alloc_header *next;
    struct alloc_header *prev;
    uint32_t            thread_id;
    uint32_t            size;
    void               *caller_pc;   // __builtin_return_address(0)
    uint32_t            alloc_seq;   // monotonic sequence number
    uint32_t            timestamp_ms;
    uint32_t            magic;       // 0xDEAD1234 — detects header corruption
} alloc_header_t;
```

The `__builtin_return_address(0)` gives the LR at the point `malloc()` was called, which combined with the elf symbol table allows pinpointing the exact source location offline.

**Recommendation:** Add a compile-time `MICROPOSIX_LEAK_CALLER_DEPTH` parameter (1–4) to capture multiple stack frames via `__builtin_return_address(N)` for harder-to-find leaks. Cost is N additional 4-byte words per allocation.

### 2.5 Garbage Collection Section — Verdict: Correctly Rejected, Expand Alternatives

The document correctly identifies that stop-the-world GC is fatal for a BLE device. A missed connection event (typically every 7.5–500ms) causes the peer to start supervision timeout countdown. GC pauses of even 2–5ms can trigger this on tight connection intervals.

The Rust borrow-checker mention is architecturally interesting but requires clarification: Rust's ownership model eliminates GC at the **language level**, not the OS level. It is a viable long-term path for the application layer but not a practical constraint on the C kernel implementation.

The three deterministic memory strategies that microPOSIX should support, in order of preference for production code:

1. **Static allocation** — all memory declared at compile time. Zero runtime cost. Cannot be depleted. Default for Profile B.
2. **Fixed-size memory pools** (`mp_alloc`/`mp_free`) — O(1), no fragmentation, ISR-safe. Primary dynamic allocator for both profiles.
3. **TLSF heap** — O(1), bounded fragmentation, suitable for variable-size objects. Profile A only, with leak detection overlay in development builds.

### 2.6 Watchdog Engine — Verdict: Excellent, One Addition Needed

The two-tier watchdog (hardware WDT + software per-thread check-in) is the correct pattern. One addition: the watchdog supervisor must record **which thread failed to check in** in retained SRAM before allowing the hardware reset. Without this, the post-reset boot message cannot identify the culprit.

```c
typedef struct {
    uint32_t magic;          // 0xBADC0FFE if valid
    uint32_t reset_cause;    // WDT, HardFault, Soft, POR
    uint32_t offending_tid;  // Thread that missed check-in (WDT reset)
    uint32_t fault_pc;       // PC at fault (HardFault reset)
    uint32_t fault_lr;
    char     thread_name[16];
} retained_crash_t __attribute__((section(".noinit")));
```

### 2.7 HardFault Handler — Verdict: Correct, Add xPSR and Stack Unwind

The register dump requirement is correct. Two additions are strongly recommended:

1. **xPSR decoding:** The xPSR value indicates whether the fault occurred in Thread or Handler mode and which exception was active. Without decoding this, a fault during an ISR is indistinguishable from a thread fault in the dump.
2. **Automated stack unwind:** On Cortex-M4F with `-fno-omit-frame-pointer` and the unwind tables (`-funwind-tables`), the crash handler can walk back 3–5 stack frames automatically, providing a mini call-stack trace without a debugger.

### 2.8 `kill <thread_id>` Command — Verdict: Requires Resource Reclamation Protocol

The `kill` command is valuable for development but introduces a serious hazard: if the killed thread holds one or more mutexes, those mutexes will never be unlocked, potentially deadlocking the remaining system. The kill implementation must:

1. Scan all registered IPC primitives for ownership by the target thread.
2. Release any held mutexes (with a warning log).
3. Free the thread's stack memory and TCB.
4. Remove the thread from all timer and block queues.
5. Mark its TID as invalid in the watchdog registry.

The implementation of safe thread cancellation is non-trivial; consider limiting `kill` to threads in `BLOCKED` or `SLEEPING` state only in the initial release, refusing to kill RUNNING or mutex-holding threads.

### 2.9 Split BLE Stack Architecture — Verdict: Correct, Clarify Vendor Constraints

The Controller/Host split with message-queue IPC is exactly the right architecture. However, **vendor BLE controllers impose RTOS constraints that must be respected**:

- **Nordic SoftDevice:** Reserves interrupt priority levels 0–1 for the SoftDevice itself. The application RTOS must use priorities 2–7 only. SysTick is borrowed by the SoftDevice and must not be reconfigured. microPOSIX must use RTC0/RTC1 as its tick source when running alongside a SoftDevice.
- **NimBLE (Apache):** Fully open-source, can run as a standard high-priority thread with a native porting layer. No priority restrictions.
- **Zephyr BT:** Tied to Zephyr's own scheduler; running it under a different RTOS requires the HCI UART transport layer as a shim.

The configuration system must have a `MICROPOSIX_BLE_BACKEND` selector that enables the correct priority mapping and tick source for each backend.

---

## 3. Resolved Architecture Decisions

The following table consolidates decisions made between the original ARD and this review.

| Topic | ARD Proposal | Resolution |
|---|---|---|
| CPU profiling mechanism | DWT cycle counter | DWT on M4F, SysTick fallback on M0+, disable on prod |
| Stack fill pattern | 0xAA | 0xAA fill + 0xDEADBEEF canary word + MPU guard on M4F |
| Memory safety | GC rejected, use pools | Static pools (B), TLSF + RC pools (A), no GC ever |
| GC alternative | Rust mention | Pool + RC ownership model in C; Rust as optional app layer |
| Tick source | SysTick | SysTick normally; RTC when SoftDevice is active |
| WDT failure record | Not specified | Retained SRAM struct with TID + thread name |
| `kill` safety | Not specified | Only kills non-mutex-holding threads; reclaims IPC resources |
| `pthread_cond_t` race | Not addressed | LDREX/STREX on M4F, critical section on M0+ |
| BLE vendor constraints | "vendor binary blobs" | Backend selector with per-vendor priority/tick mapping |
| Leak header | TID + size + PC | TID + size + PC (LR via `__builtin_return_address`) + timestamp + magic |

---

## 4. Complete microPOSIX Feature Catalogue

### 4.1 Kernel and Scheduler

| Feature | Profile A (90MHz) | Profile B (20MHz) | Notes |
|---|---|---|---|
| Preemptive priority scheduling | ✅ 32 priorities | ✅ 8 priorities | Fixed-priority, BLE at P31 |
| Cooperative scheduling mode | ✅ Optional | ✅ Default | Toggle via Kconfig |
| Round-robin among equal priorities | ✅ Configurable quantum | ✅ 50ms quantum | |
| Tickless idle (dynamic sleep) | ✅ Full | ✅ Full | BLE timing oracle integration |
| Priority inheritance on mutexes | ✅ Full | ⚠️ Optional | Saves ~500B flash on B |
| SysTick resolution | 1ms | 10ms | |
| 64-bit monotonic uptime counter | ✅ | ✅ | µs resolution on A, ms on B |
| Max concurrent threads | 32 | 8 | Static TCB pool |
| Thread stack alignment | 8-byte (M4F ABI) | 4-byte (M0+ ABI) | |
| SMP (dual-core) support | ✅ Optional | ❌ | RP2040, ESP32-S3 |

### 4.2 POSIX API Surface

| API | Profile A | Profile B | Notes |
|---|---|---|---|
| `pthread_create / join / detach` | ✅ | ✅ | |
| `pthread_cancel` | ✅ | ⚠️ | Safe cancel with resource reclaim |
| `pthread_mutex_t` | ✅ Full | ✅ Full | Recursive + PI variants |
| `pthread_cond_t` | ✅ Full | ✅ Simplified | LDREX/STREX race-free impl |
| `sem_open / sem_wait / sem_post` | ✅ | ✅ | ISR-safe post |
| `mq_open / mq_send / mq_receive` | ✅ | ✅ | Fixed-depth queues |
| `clock_gettime (MONOTONIC)` | ✅ | ✅ | Backed by 64-bit counter |
| `nanosleep / usleep` | ✅ | ⚠️ ms granularity | |
| `pthread_setschedparam` | ✅ | ❌ | Priority change at runtime |
| `sigaction / signal` | ⚠️ Subset | ❌ | SIGSEGV, SIGABRT mapped to fault |

### 4.3 Thread Management Engine (TME)

| Feature | Profile A | Profile B | Notes |
|---|---|---|---|
| Thread Control Block (TCB) | Full (72B) | Lite (24B) | Full TCB has all counters |
| CPU utilisation per thread (%) | ✅ DWT-based | ⚠️ SysTick coarse | 1s rolling window |
| Stack high-water-mark | ✅ 0xAA scan | ✅ 0xAA scan | Background task, 1Hz |
| Stack canary check on switch | ✅ | ✅ | 0xDEADBEEF at stack bottom |
| Per-thread heap bytes (current) | ✅ | ⚠️ Count only | Tracked via malloc wrapper |
| Per-thread heap bytes (peak) | ✅ | ❌ | |
| Thread lifecycle event log | ✅ 256-entry ring | ✅ 32-entry ring | CREATE/RUN/BLOCK/CRASH etc. |
| Context switch count per thread | ✅ | ✅ | |
| Last scheduled timestamp | ✅ | ❌ | |
| `nimble_tme_dump()` UART output | ✅ Full table | ✅ Minimal | Within 10ms |
| Thread suspend/resume API | ✅ | ✅ | |
| CPU budget enforcement | ✅ | ❌ | Throttle threads over quota |
| `top` command | ✅ | ❌ | Real-time refresh via UART0 |

### 4.4 Memory Management

| Feature | Profile A | Profile B | Notes |
|---|---|---|---|
| TLSF heap allocator | ✅ O(1) | ❌ | Bounded fragmentation |
| Fixed-size pool allocator | ✅ | ✅ | O(1) alloc/free, ISR-safe |
| Reference-counted pool (rcpool) | ✅ | ❌ | Zero-copy BLE/LCD buffer sharing |
| Static-only allocation mode | ✅ Optional | ✅ Default | |
| Per-allocation header (leak track) | ✅ Dev mode | ❌ | 28B overhead per allocation |
| Leak detector background task | ✅ | ❌ | Scans at 5Hz, age threshold |
| Leak report via UART0 (`mem`) | ✅ | ❌ | File:line + age + hex preview |
| Stack MPU guard region | ✅ 32B | ❌ | Hardware fault on overflow |
| Heap fragmentation metric | ✅ | ❌ | Reported in `mem` command |
| Retained SRAM noinit section | ✅ | ✅ | Crash record survives soft reset |
| Flash crash log (8 records) | ✅ | ❌ | Survives power cycle |

### 4.5 Debug and Logging Engine

| Feature | Profile A | Profile B | Notes |
|---|---|---|---|
| UART0 DMA-driven TX | ✅ 4KB buffer | ✅ 512B buffer | Non-blocking from all threads |
| Baud rate | 921600 | 115200 | Configurable |
| Log levels (VRB/DBG/INF/WRN/ERR/CRT) | ✅ | ✅ WRN+ only | Per-module filtering on A |
| Timestamp in every log line | ✅ µs | ✅ ms | |
| Thread name in every log line | ✅ | ✅ | |
| Lock-free ring buffer for log writes | ✅ | ✅ | ISR-safe writes |
| Binary structured log mode | ✅ Optional | ❌ | Deferred string table, low bandwidth |
| `hexdump()` helper | ✅ | ⚠️ 16B only | |
| Zero-overhead release build | ✅ | ✅ | All macros → NOP at LOG_LEVEL=NONE |
| UART0 RX command shell | ✅ Full | ❌ | Low-priority background thread |
| Module-level runtime log control | ✅ | ❌ | `log ble_mgr debug` in shell |

### 4.6 UART0 Debug Shell Commands

| Command | Description | Profile |
|---|---|---|
| `top` | Real-time CPU% + stack usage per thread (refreshes) | A only |
| `threads` | Static snapshot of all thread states and metrics | A only |
| `thread <id>` | Detailed single-thread info | A only |
| `events [id]` | TME lifecycle event history | A only |
| `mem` | Heap stats + fragmentation + leak detector trigger | A only |
| `leaks` | Full allocation list with call site and age | A only |
| `ble stat` | Connection interval, RSSI, PHY, MTU, dropped packets | Both |
| `ble adv <fast\|slow\|off>` | Change advertising mode | Both |
| `lcd` | Framebuffer status and FPS counter | A only |
| `log <module> <level>` | Change log level at runtime | A only |
| `kill <tid>` | Safely terminate a thread (non-mutex-holding) | A only |
| `reboot` | Controlled software reset | Both |
| `reset wdt` | Force WDT test (withholds feed) | Both |
| `assert` | Deliberate fault for crash handler testing | Both |
| `uptime` | System uptime + reset cause | Both |
| `version` | OS version, build date, git hash | Both |
| `help` | List all commands | Both |

### 4.7 Crash and Fault Tolerance

| Feature | Profile A | Profile B | Notes |
|---|---|---|---|
| Hardware WDT (IWDG/WDT) | ✅ 5s timeout | ✅ 8s timeout | Armed at boot |
| Software WDT (per-thread check-in) | ✅ Per-thread | ✅ Global | Supervisor at P27 |
| WDT failure identity in retained SRAM | ✅ | ✅ | TID + name + timestamp |
| HardFault handler | ✅ Full dump | ✅ Minimal dump | |
| MemManage / BusFault / UsageFault | ✅ | ❌ Not on M0+ | |
| Register dump (R0–R15, xPSR) | ✅ | ✅ | |
| Automated stack unwind (3 frames) | ✅ | ❌ | Requires `-funwind-tables` |
| xPSR mode decode | ✅ | ❌ | Thread vs Handler mode |
| Crash report over UART0 | ✅ Full | ✅ Minimal | Emitted before reboot |
| Retained SRAM crash record | ✅ | ✅ | 64B, printed on next boot |
| Flash crash log | ✅ 8 records | ❌ | Post-mortem history |
| User crash hooks | ✅ | ✅ | BLE disconnect, LCD error screen |
| Reboot delay after crash | ✅ 3s (configurable) | ✅ 1s | 0 = halt for JTAG attach |
| `MICROPOSIX_ASSERT(cond)` | ✅ Full | ✅ Minimal | |
| `MICROPOSIX_PANIC(fmt, ...)` | ✅ | ✅ | |

### 4.8 IPC and Synchronisation Primitives

| Primitive | Profile A | Profile B | ISR-Safe | Notes |
|---|---|---|---|---|
| Mutex (recursive) | ✅ | ✅ | Post only | Priority inheritance optional |
| Binary semaphore | ✅ | ✅ | ✅ | |
| Counting semaphore | ✅ | ✅ | ✅ | |
| Message queue (fixed-size items) | ✅ | ✅ | ✅ | |
| Event flags (32-bit, AND/OR wait) | ✅ | ✅ | ✅ | |
| Thread notification word | ✅ | ✅ | ✅ | Lightest wakeup mechanism |
| Lock-free SPSC ring buffer | ✅ | ✅ | ✅ | ISR→thread data paths |
| Lock-free MPMC ring buffer | ✅ | ❌ | ✅ | Multi-thread stream exchange |
| Software timers (one-shot/periodic) | ✅ 16 max | ✅ 8 max | Set from ISR | |
| Condition variables (`pthread_cond_t`) | ✅ | ✅ | ❌ | Race-free impl |

### 4.9 BLE Management Subsystem

| Feature | Profile A | Profile B | Notes |
|---|---|---|---|
| BLE Peripheral role | ✅ | ✅ | |
| BLE Central role | ✅ | ❌ | Footprint too large for B |
| BLE Broadcaster (advert-only) | ✅ | ✅ | |
| Max simultaneous connections | 4 | 1 | |
| GATT server declarative macros | ✅ | ✅ | Static attribute table |
| Dynamic GATT service registration | ✅ | ❌ | |
| Notification throughput target | ≥4 KB/s | ≥1 KB/s | At 7.5ms CI, 247B MTU |
| Connection parameter update | ✅ | ✅ | |
| Auto-reconnect with back-off | ✅ | ✅ | |
| BLE TX power control | ✅ | ✅ | |
| PHY selection (1M / 2M / Coded) | ✅ | ⚠️ 1M only | Vendor stack dependent |
| BLE sleep between connection events | ✅ | ✅ | Tickless idle integration |
| HCI UART shim (for Zephyr BT) | ✅ | ❌ | |
| NimBLE native backend | ✅ | ✅ | |
| Nordic SoftDevice backend | ✅ | ✅ | RTC tick source adaptation |
| BLE stat shell command | ✅ | ✅ | |
| BLE data IPC queues | ✅ | ✅ | App↔Host decoupling |

### 4.10 LCD / Display Subsystem

| Feature | Profile A | Profile B | Notes |
|---|---|---|---|
| SPI display drivers (ILI9341, ST7789) | ✅ | ❌ | |
| OLED tile driver (SSD1306 128×64) | ✅ | ⚠️ Optional | 1KB framebuffer |
| Double-buffered framebuffer | ✅ | ❌ | 150KB @ 320×240 RGB565 |
| DMA framebuffer transfer | ✅ | ❌ | Zero CPU during transfer |
| 2D graphics primitives | ✅ | ❌ | Rect, line, char, string, bitmap |
| Fixed-width fonts (6×8, 8×12, 12×16) | ✅ | ❌ | Flash-resident |
| `lcd_printf()` overlay | ✅ | ❌ | Debug overlays on display |
| Headless build (no LCD) | ✅ | ✅ | `MICROPOSIX_LCD_ENABLE=0` |
| FPS counter | ✅ | ❌ | Via TME |

### 4.11 Power Management

| Feature | Profile A | Profile B | Notes |
|---|---|---|---|
| Idle sleep (WFI in idle thread) | ✅ | ✅ | Automatic |
| Light sleep (BLE polling) | ✅ | ✅ | |
| Deep sleep (SRAM retained) | ✅ | ✅ | |
| Hibernate (RTC wake only) | ✅ | ✅ | |
| Power constraint voting system | ✅ | ✅ | Deepest state where all constraints met |
| RTC wake across deep sleep | ✅ | ✅ | |
| BLE-aware sleep scheduling | ✅ | ✅ | Next anchor point calculation |
| Power profiling mode | ✅ Optional | ❌ | Logs estimated µA per state |

---

## 5. 90MHz Headroom Analysis

This section provides a quantitative breakdown of the CPU budget consumed by microPOSIX on a Cortex-M4F at 90MHz. At 90MHz, the processor executes approximately **90 million clock cycles per second**. Each subsystem's overhead is measured in cycles/second to give a percentage of total capacity.

### 5.1 Context Switch Overhead

A context switch on Cortex-M4F requires saving/restoring the full register file including FPU state (S0–S15 lazy-stacked). This takes approximately 200–350 cycles per switch.

```
Context switch cost:    ~250 cycles (with FPU lazy stacking)
Assumed switch rate:    200 switches/second (busy system with BLE + UI + sensors)
Annual cycle cost:      250 × 200 = 50,000 cycles/sec
CPU overhead:           50,000 / 90,000,000 = 0.056%
```

### 5.2 DWT-Based CPU Profiling

At every context switch, the TME reads `DWT->CYCCNT` (single LDR instruction, 1–2 cycles) and stores the delta in the TCB.

```
Per-switch DWT cost:    ~4 cycles (read + subtract + store)
At 200 switches/sec:    800 cycles/sec
CPU overhead:           ~0.001%
```

### 5.3 BLE Stack Processing

A BLE 5.0 connection at 7.5ms connection interval requires the controller thread to process each connection event.

```
BLE connection event processing:    ~2,000–5,000 cycles per event
Connection interval:                7.5ms → 133 events/sec
Cycles for controller:              5,000 × 133 = 665,000 cycles/sec
BLE host (GATT/ATT processing):     ~1,000,000 cycles/sec (1 KB/s throughput)
Total BLE overhead:                 ~1,665,000 cycles/sec
CPU overhead:                       1,665,000 / 90,000,000 = 1.85%
```

### 5.4 UART0 DMA Logging

At 921600 baud, the UART can emit approximately 115,200 bytes/second. With DMA, the CPU is involved only in:
- Ring buffer write from logging thread: ~20 cycles per log entry
- DMA completion ISR: ~50 cycles per DMA transfer (every 4KB drain)

```
Log entries per second:     500 (busy debug session)
Ring buffer write cost:     500 × 20 = 10,000 cycles/sec
DMA transfers per sec:      ~28 (4KB chunks at 115KB/s)
ISR handling:               28 × 50 = 1,400 cycles/sec
Total UART overhead:        ~11,400 cycles/sec
CPU overhead:               ~0.013%
```

### 5.5 Stack High-Water-Mark Scanner

The scanner walks stack bytes looking for the first non-`0xAA` byte. For a 512-byte stack (128 32-bit words), the worst-case scan is 128 word comparisons.

```
512B stack scan cost:       ~150 cycles per thread
Threads:                    16
Scan interval:              1,000ms
Cycles per second:          16 × 150 / 1 = 2,400 cycles/sec
CPU overhead:               ~0.003%
```

### 5.6 Heap Leak Detector

The detector traverses a linked list of live allocations. In a typical development session with 200 live allocations, a traversal takes roughly 200 × 10 cycles.

```
Allocation count:           200 (development session)
Traversal cost:             200 × 10 = 2,000 cycles
Scan frequency:             0.2/sec (every 5 seconds)
Cycles per second:          2,000 × 0.2 = 400 cycles/sec
CPU overhead:               ~0.000%
```

### 5.7 Watchdog Supervisor

The WDT supervisor wakes every 1,000ms, checks 8 thread check-in flags, and issues a hardware feed instruction.

```
Check-in scan:              8 × 5 cycles = 40 cycles per wake
Wake frequency:             1/sec
Cycles per second:          40
CPU overhead:               ~0.000%
```

### 5.8 LCD DMA Framebuffer Transfer

At 320×240×2 bytes = 150KB per frame at 30fps, the SPI DMA transfers 4.5 MB/sec. CPU involvement is only the DMA setup and completion ISR.

```
DMA setup per frame:        ~200 cycles
Completion ISR:             ~100 cycles
Frame rate:                 30fps
Cycles per second:          30 × 300 = 9,000 cycles/sec
CPU overhead:               ~0.010%
```

### 5.9 TME Event Ring Buffer

Every thread state transition writes a 32-byte event record to the ring buffer. At 200 context switches + 50 block/unblock events per second:

```
Events per second:          250
Write cost per event:       ~30 cycles (memcpy + index update)
Cycles per second:          250 × 30 = 7,500 cycles/sec
CPU overhead:               ~0.008%
```

### 5.10 Total Overhead Summary

| Subsystem | Cycles/sec | CPU % at 90MHz |
|---|---|---|
| Context switching (200/s) | 50,000 | 0.056% |
| DWT profiling hook | 800 | 0.001% |
| BLE controller + host | 1,665,000 | 1.850% |
| UART0 DMA logging | 11,400 | 0.013% |
| Stack HWM scanner | 2,400 | 0.003% |
| Heap leak detector (dev) | 400 | <0.001% |
| Watchdog supervisor | 40 | <0.001% |
| LCD DMA management | 9,000 | 0.010% |
| TME event ring writes | 7,500 | 0.008% |
| Scheduler tick (1ms SysTick) | 5,000 | 0.006% |
| **Total OS Overhead** | **~1,752,000** | **~1.95%** |
| **Available to Application** | **~88,248,000** | **~98.05%** |

### 5.11 Headroom Interpretation

At **90MHz with the full feature set enabled**, microPOSIX consumes approximately **2% of CPU capacity**. The remaining **98% — equivalent to approximately 88 million cycles per second** — is available for application logic. This headroom is sufficient for:

- **High-frequency sensor fusion:** IMU at 1kHz with Madgwick/Mahony filter (~10,000 cycles per iteration = 1% additional load)
- **Signal processing:** 512-point FFT in ~50,000 cycles = <0.1% at 10Hz update rate
- **BLE data pipeline:** Formatting and queuing 4KB/s of application data = <0.5% additional
- **Display rendering:** Drawing a full 320×240 UI frame in software (~800,000 cycles/frame) at 30fps = ~26% — this is the largest potential consumer and argues strongly for DMA-driven display updates
- **ML inference:** A small CMSIS-NN neural network (~500K cycles/inference) at 5Hz = ~2.8% additional

**Practical ceiling for this hardware at 90MHz:**
A BLE HMI application running a sensor fusion loop at 500Hz, refreshing a 320×240 display at 30fps via DMA, and maintaining a BLE connection at 7.5ms CI with 2KB/s throughput uses approximately **30–35% total CPU**, leaving an additional **60–65% margin** before performance issues emerge. The microPOSIX overhead itself is not a limiting factor at this clock rate.

---

## 6. Scaling Analysis: 90MHz vs 20MHz

The quantitative difference between profiles is stark. At 20MHz (Cortex-M0+), clock cycles are 4.5× more expensive per unit of time.

| Metric | 90MHz Full | 20MHz Minimal | Ratio |
|---|---|---|---|
| Clock cycles/sec | 90,000,000 | 20,000,000 | 4.5× |
| Context switch time | ~2.8µs | ~10µs | 3.6× slower |
| BLE overhead (% CPU) | ~1.85% | ~8.3% | 4.5× more |
| OS total overhead | ~2% | ~10-12% | ~6× more |
| Available for application | ~98% | ~88–90% | — |
| Max useful threads | 32 | 8 | 4× fewer |
| DWT profiling | ✅ Available | ❌ Not present | — |
| LCD 30fps (software) | ~26% additional | ❌ Infeasible | |
| LCD 30fps (DMA) | ~0.01% additional | ~0.05% additional | DMA is mandatory |

The 20MHz profile is appropriate for BLE sensor nodes, BLE remote controls, and headless IoT endpoints where the application logic is lightweight. It is not suitable for real-time display rendering, BLE Central with multiple connections, or computationally intensive sensor fusion.

**The 20MHz build stripping rule:** Every Kconfig flag that removes a feature on Profile B saves both flash and RAM. The flash savings from disabling the CLI shell (~6KB), the TLSF allocator (~3KB), and the leak detector (~2KB) free approximately 11KB of flash — critical on a 64KB device.

---

## 7. Critical Implementation Risks and Mitigations

### 7.1 BLE Radio Interrupt Interference

**Risk:** A kernel critical section longer than the BLE link layer's timing window (as short as 150µs at 1M PHY) will cause a missed connection event and ultimately a supervision timeout.

**Mitigation:** All kernel critical sections use `__disable_irq()` / `__enable_irq()` with a maximum hold time budget of 10µs. Any operation longer than 10µs must be broken into interruptible segments. A DWT-based maximum critical section duration assertion should be enabled in development builds.

### 7.2 Stack Overflow Before MPU Detection

**Risk:** On Profile B (no MPU), a stack overflow silently corrupts the heap or adjacent stacks before the canary check fires on the next context switch.

**Mitigation:** Generously oversize all stacks by 20% during development. Use the HWM scanner data to right-size for production. Set `MICROPOSIX_STACK_OVERFLOW_POLICY=PANIC` so that any canary modification immediately crashes with a diagnostic rather than silently corrupting memory.

### 7.3 `pthread_cond_t` Priority Inversion

**Risk:** A high-priority thread waiting on a condition variable may be starved if the low-priority thread signalling the condition is preempted while holding the associated mutex.

**Mitigation:** Condition variables must always be used with a mutex that has priority inheritance enabled (`MICROPOSIX_MUTEX_PI_ENABLE=1`). This is enforced by the `pthread_cond_wait()` implementation which asserts that the mutex has PI enabled at runtime in debug builds.

### 7.4 Vendor SoftDevice Tick Conflict

**Risk:** Nordic SoftDevice seizes the SysTick timer for its own timing. Using SysTick for the microPOSIX scheduler tick will cause undefined behaviour or hard faults.

**Mitigation:** When `MICROPOSIX_BLE_BACKEND=SOFTDEVICE` is selected, the Kconfig system automatically sets `MICROPOSIX_TICK_SOURCE=RTC1` and adjusts tick resolution accordingly. This switch is transparent to all OS code above the HAL.

### 7.5 `kill` Leaving Orphaned Resources

**Risk:** Killing a thread that holds a mutex, has pending queue messages, or owns pool allocations leaves the system in an inconsistent state.

**Mitigation:** `mp_kill(tid)` triggers a sequential resource audit:
1. Walk all mutex ownership tables and release any held by `tid`.
2. Walk all message queues and mark messages from `tid` as stale.
3. Walk the heap allocation list and free all blocks owned by `tid`.
4. Remove `tid` from the WDT registry.
5. Emit a warning for each released resource.

This makes `kill` safe for development use. Production code should never `kill` a thread; instead, use cancellation points and `pthread_cancel()`.

---

## 8. Recommended Implementation Order

The development roadmap is sequenced to make each phase independently testable on the POSIX simulation backend (`platform/posix`) before touching hardware.

### Phase 1 — Kernel Skeleton (Weeks 1–4)
- Cooperative and preemptive scheduler on POSIX simulation
- TCB, state machine, context switch (setjmp/longjmp on POSIX, PendSV on Cortex-M)
- `pthread_create`, `pthread_join`, `pthread_mutex_t`, `sem_post/wait`
- Software timers, 64-bit monotonic clock
- Unit tests: 100% coverage on scheduler state transitions

### Phase 2 — Memory and Debug (Weeks 5–8)
- TLSF heap with per-allocation header
- Pool allocator (both profiles)
- UART0 DMA log engine with lock-free ring buffer
- TME: CPU utilisation (DWT), stack HWM, event ring
- Watchdog engine: hardware WDT + per-thread check-in
- HardFault handler with register dump and retained SRAM record

### Phase 3 — BLE Integration (Weeks 9–14)
- NimBLE backend porting layer
- BLE Manager: peripheral mode, GATT server macros, connection lifecycle
- Message queue IPC for app↔BLE data exchange
- Tickless idle with BLE timing oracle integration
- SoftDevice backend adaptation (RTC tick source, priority mapping)
- BLE `stat` command in debug shell

### Phase 4 — Shell and Advanced Profiling (Weeks 15–18)
- UART0 RX shell: `top`, `mem`, `leaks`, `kill`, `ble stat`
- Leak detector background task
- RC pool allocator
- CPU budget enforcement
- Profile B stripping and footprint validation

### Phase 5 — LCD and Power (Weeks 19–22)
- LCD driver: SPI DMA, double-buffer, ILI9341/ST7789
- 2D primitives and font rendering
- Power state machine with constraint voting
- Power profiling mode
- Validation on nRF52840 and STM32WB55

### Phase 6 — Hardening (Weeks 23–26)
- MPU-backed stack guards
- Flash crash log (8-record history)
- Automated stack unwind in crash handler
- Full test suite on Profile B (nRF52810, EFR32BG22)
- Memory footprint audit and optimisation

---

## 9. Toolchain and Build System

### 9.1 Toolchain Requirements

```
Compiler:   arm-none-eabi-gcc ≥ 12.x  (C11, -Wall -Wextra -Werror)
Linker:     arm-none-eabi-ld with custom linker script per target
Libraries:  newlib-nano (no printf float unless MICROPOSIX_PRINTF_FLOAT=1)
Debugger:   OpenOCD + arm-none-eabi-gdb / J-Link GDB Server
Host tests: GCC x86_64 with POSIX simulation backend
```

### 9.2 Kconfig Feature Matrix for Build System

```
# microposix/Kconfig (top-level excerpt)

config MICROPOSIX_PROFILE
    int "Hardware profile (0=Minimal/20MHz, 1=Full/90MHz)"
    default 1

config MICROPOSIX_SCHED_PREEMPTIVE
    bool "Enable preemptive scheduling"
    default y if MICROPOSIX_PROFILE=1
    default n if MICROPOSIX_PROFILE=0

config MICROPOSIX_CPU_PROFILE
    bool "Enable DWT cycle counter CPU profiling"
    depends on MICROPOSIX_PROFILE=1
    depends on ARM_ARCH_7EM  # Cortex-M4/M7 only

config MICROPOSIX_LEAK_DETECT
    bool "Enable heap leak detection (development only)"
    depends on MICROPOSIX_PROFILE=1
    select MICROPOSIX_HEAP_TLSF

config MICROPOSIX_LCD_ENABLE
    bool "Enable LCD display subsystem"
    depends on MICROPOSIX_PROFILE=1

config MICROPOSIX_SHELL_ENABLE
    bool "Enable UART0 interactive debug shell"
    depends on MICROPOSIX_PROFILE=1
```

### 9.3 Compiler Flags for Profiles

```makefile
# Profile A — 90MHz Cortex-M4F
CFLAGS_A = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
           -O2 -ffunction-sections -fdata-sections \
           -funwind-tables -fno-omit-frame-pointer \
           -DMICROPOSIX_PROFILE=1

# Profile B — 20MHz Cortex-M0+
CFLAGS_B = -mcpu=cortex-m0plus -mthumb \
           -Os -ffunction-sections -fdata-sections \
           -DMICROPOSIX_PROFILE=0 \
           -DMICROPOSIX_SCHED_PREEMPTIVE=0 \
           -DMICROPOSIX_CPU_PROFILE=0 \
           -DMICROPOSIX_LEAK_DETECT=0 \
           -DMICROPOSIX_SHELL_ENABLE=0
```

---

## 10. Suggested Target MCU Board Matrix

| Board / SoC | Profile | BLE | Notes |
|---|---|---|---|
| Nordic nRF52840-DK | A (64MHz) | ✅ BLE 5.3 | SoftDevice or NimBLE; 1MB flash, 256KB RAM |
| Nordic nRF52840 (custom PCB) | A | ✅ BLE 5.3 | Primary development target |
| STM32WB55 Nucleo | A (64MHz) | ✅ BLE 5.0 | STM32 WPAN stack; dual-core M4+M0+ |
| RP2040 + CYW43439 | A (133MHz) | ✅ BLE 5.2 | Dual-core SMP; Pico W compatible |
| ESP32-C3 | A (160MHz) | ✅ BLE 5.0 | RISC-V; NimBLE; good entry-level target |
| Nordic nRF52810 | B (16MHz) | ✅ BLE 5.0 | Minimal profile validation target |
| Silicon Labs EFR32BG22 | B (38MHz) | ✅ BLE 5.3 | Ultra-low-power validation |
| STM32L433 + ext. BLE | B (80MHz) | ⚠️ Via SPI | If using external HCI module (HM-10, NINA-B1) |

---

## 11. Final Architecture Diagram

```
                    microPOSIX Full Architecture (Profile A)
┌─────────────────────────────────────────────────────────────────────────────┐
│                         USER APPLICATION LAYER                              │
│   app_main()  │  sensor_fusion.c  │  ui_manager.c  │  data_logger.c        │
├─────────────────────────────────────────────────────────────────────────────┤
│                        SERVICE / MIDDLEWARE LAYER                           │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌─────────────┐  │
│  │  BLE Mgr  │ │  LCD Drv  │ │  Storage  │ │  RC Pools │ │  Power Mgr  │  │
│  │  (P24-26) │ │  (P12)    │ │           │ │ (zerocopy)│ │             │  │
│  └───────────┘ └───────────┘ └───────────┘ └───────────┘ └─────────────┘  │
├─────────────────────────────────────────────────────────────────────────────┤
│                     OBSERVABILITY / DEBUG LAYER                             │
│  ┌───────────────┐ ┌──────────────┐ ┌─────────────┐ ┌────────────────────┐│
│  │  UART0 Shell  │ │  Log Engine  │ │  Crash Hdlr │ │  WDT Supervisor    ││
│  │  (P1)         │ │  Lock-free   │ │  HardFault  │ │  (P27)             ││
│  │  top/mem/kill │ │  DMA TX buf  │ │  Retained   │ │  Per-thread check-in│
│  └───────────────┘ └──────────────┘ └─────────────┘ └────────────────────┘│
├───────────────────────────────┬─────────────────────────────────────────────┤
│     THREAD MANAGEMENT ENGINE  │           MEMORY MANAGEMENT                 │
│  ┌────────────────────────┐   │  ┌──────────┐ ┌──────────┐ ┌────────────┐ │
│  │ TCB Array              │   │  │  TLSF    │ │  Fixed   │ │  Leak      │ │
│  │ DWT CPU profiling      │   │  │  Heap    │ │  Pools   │ │  Detector  │ │
│  │ Stack HWM scanner      │   │  │  (O(1))  │ │  (O(1))  │ │  Linked    │ │
│  │ Event ring buffer      │   │  │          │ │ ISR-safe │ │  List      │ │
│  │ Per-thread accounting  │   │  └──────────┘ └──────────┘ └────────────┘ │
│  └────────────────────────┘   │                                             │
├───────────────────────────────┴─────────────────────────────────────────────┤
│                            KERNEL LAYER                                     │
│  ┌──────────────────┐  ┌────────────────┐  ┌──────────┐  ┌───────────────┐│
│  │  Preemptive      │  │  IPC:          │  │  SW      │  │  ISR Deferred ││
│  │  Scheduler       │  │  mutex/sem/q   │  │  Timers  │  │  Handler      ││
│  │  PendSV ctx-sw   │  │  events/notif  │  │  (timer  │  │  (P28)        ││
│  │  Priority inherit│  │  SPSC/MPMC ring│  │   task)  │  │               ││
│  └──────────────────┘  └────────────────┘  └──────────┘  └───────────────┘│
├─────────────────────────────────────────────────────────────────────────────┤
│                    HARDWARE ABSTRACTION LAYER (HAL)                         │
│   UART0 (DMA) │ BLE Radio │ SPI/I2C │ GPIO │ RTC │ DWT │ MPU │ WDT       │
├─────────────────────────────────────────────────────────────────────────────┤
│          ARM Cortex-M4F @ 90MHz │ FPU │ MPU │ DWT │ NVIC (240 levels)     │
└─────────────────────────────────────────────────────────────────────────────┘

Thread Priority Map:
 P31 │ BLE Controller (radio ISR follow-up)
 P28 │ OS reserved / ISR deferred handler
 P27 │ WDT Supervisor
P24-26│ BLE Host (GAP/GATT)
P20-23│ BLE TX pipeline / high-rate sensors
P16-19│ Sensor fusion / real-time data
P12-15│ LCD driver / DMA commit
 P8-11│ UI manager / touch handler
  P4-7│ Application business logic
  P2-3│ Log drain / UART0 DMA flush
   P1 │ Debug shell (UART0 RX)
   P0 │ Idle (WFI / tickless sleep)
```

---

*microPOSIX Final Architecture Document v1.0*
*Based on submitted ARD review, NimbleOS SRS, and 90MHz headroom analysis*
*For: Embedded BLE HMI firmware development*
