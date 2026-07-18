# nRF54L15 Platform CMake Configuration
# 
# This file configures the build for Nordic nRF54L15 dual-core (Cortex-M33 + RISC-V).
# Supports both Asymmetric Multi-Processing (AMP) and optional SMP.

# Platform-specific options
option(MICROPOSIX_NRF54L15_AMP "Enable Asymmetric Multi-Processing (AMP) mode" ON)
option(MICROPOSIX_NRF54L15_SMP "Enable Symmetric Multi-Processing (SMP) mode" OFF)

# Core selection (required)
if(NOT DEFINED MICROPOSIX_CORE_M33 AND NOT DEFINED MICROPOSIX_CORE_RV32)
    message(WARNING "nRF54L15 requires MICROPOSIX_CORE_M33 or MICROPOSIX_CORE_RV32. Defaulting to M33.")
    set(MICROPOSIX_CORE_M33 ON)
endif()

# Validate core selection
if(MICROPOSIX_CORE_M33 AND MICROPOSIX_CORE_RV32)
    message(FATAL_ERROR "Cannot build for both M33 and RV32 cores simultaneously. Use separate build directories.")
endif()

# Set core-specific definitions
if(MICROPOSIX_CORE_M33)
    add_compile_definitions(MICROPOSIX_CORE_M33=1)
    add_compile_definitions(MICROPOSIX_CORE_ID=0)
    set(CORE_NAME "m33")
elseif(MICROPOSIX_CORE_RV32)
    add_compile_definitions(MICROPOSIX_CORE_RV32=1)
    add_compile_definitions(MICROPOSIX_CORE_ID=1)
    set(CORE_NAME "rv32")
else()
    message(FATAL_ERROR "nRF54L15 requires MICROPOSIX_CORE_M33 or MICROPOSIX_CORE_RV32")
endif()

# AMP/SMP mode
if(MICROPOSIX_NRF54L15_AMP)
    add_compile_definitions(MICROPOSIX_NRF54L15_AMP=1)
endif()

if(MICROPOSIX_NRF54L15_SMP)
    add_compile_definitions(MICROPOSIX_NRF54L15_SMP=1)
endif()

# nRF54L15-specific includes
set(NRF54L15_INCLUDE_DIRS
    include/microposix/hal/nrf54l15
    include/microposix/hal/nrf54l15/shared
    include/microposix/hal/nrf54l15/${CORE_NAME}
)

# nRF54L15-specific sources
set(NRF54L15_SRC_SHARED
    platform/nrf54l15/shared/ipc.c
    platform/nrf54l15/shared/shared_memory.c
    platform/nrf54l15/shared/core_sync.c
)

set(NRF54L15_SRC_CORE
    platform/nrf54l15/${CORE_NAME}/context_switch.c
    platform/nrf54l15/${CORE_NAME}/cpu.c
    platform/nrf54l15/${CORE_NAME}/startup.c
    platform/nrf54l15/${CORE_NAME}/irq_handlers.c
)

# Combine all platform sources
set(MICROPOSIX_SRC_PLATFORM
    ${NRF54L15_SRC_SHARED}
    ${NRF54L15_SRC_CORE}
)

# Add platform-specific include directories to parent scope
set(MICROPOSIX_INCLUDE_DIRS ${MICROPOSIX_INCLUDE_DIRS} ${NRF54L15_INCLUDE_DIRS} PARENT_SCOPE)
set(MICROPOSIX_SRC_PLATFORM ${MICROPOSIX_SRC_PLATFORM} PARENT_SCOPE)

# Core-specific compiler flags
if(MICROPOSIX_CORE_M33)
    # Cortex-M33 flags
    set(CORE_C_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")
    set(CORE_ASM_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")
    set(CORE_LINKER_SCRIPT "platform/nrf54l15/m33/linker.ld")
elseif(MICROPOSIX_CORE_RV32)
    # RISC-V flags (RV32IMAC)
    set(CORE_C_FLAGS "-march=rv32imac -mabi=ilp32")
    set(CORE_ASM_FLAGS "-march=rv32imac -mabi=ilp32")
    set(CORE_LINKER_SCRIPT "platform/nrf54l15/rv32/linker.ld")
endif()

# Apply core-specific flags to parent scope
set(CORE_C_FLAGS ${CORE_C_FLAGS} PARENT_SCOPE)
set(CORE_ASM_FLAGS ${CORE_ASM_FLAGS} PARENT_SCOPE)
set(CORE_LINKER_SCRIPT ${CORE_LINKER_SCRIPT} PARENT_SCOPE)
