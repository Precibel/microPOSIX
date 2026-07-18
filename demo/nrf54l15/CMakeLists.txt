# nRF54L15 Demo Application CMakeLists.txt
#
# This file configures the demo application for the nRF54L15 dual-core SoC.

# Demo-specific options
option(DEMO_M33 "Build demo for Cortex-M33 core" ON)
option(DEMO_RV32 "Build demo for RISC-V core" ON)
option(DEMO_IPC_TEST "Enable IPC test in demo" ON)
option(DEMO_THREAD_TEST "Enable thread test in demo" ON)
option(DEMO_SYNC_TEST "Enable synchronization test in demo" ON)

# Set the platform
set(MICROPOSIX_PLATFORM nrf54l15)

# Include the main microPOSIX CMake file
include(${CMAKE_SOURCE_DIR}/CMakeLists.txt)

# Demo source files
set(DEMO_SRC
    demo/nrf54l15/main.c
    demo/nrf54l15/ipc_test.c
    demo/nrf54l15/thread_test.c
    demo/nrf54l15/sync_test.c
)

# Add the demo executable
if(DEMO_M33)
    # Cortex-M33 demo
    set(DEMO_NAME "nrf54l15_m33_demo")
    add_executable(${DEMO_NAME} ${DEMO_SRC})
    
    # Set core-specific definitions
    target_compile_definitions(${DEMO_NAME} PRIVATE MICROPOSIX_CORE_M33=1)
    
    # Include directories
    target_include_directories(${DEMO_NAME} PRIVATE
        ${MICROPOSIX_INCLUDE_DIRS}
        demo/nrf54l15
    )
    
    # Link with microPOSIX
    target_link_libraries(${DEMO_NAME} microposix)
    
    # Set output name
    set_target_properties(${DEMO_NAME} PROPERTIES
        OUTPUT_NAME "nrf54l15_m33_demo"
        SUFFIX ".elf"
    )
endif()

if(DEMO_RV32)
    # RISC-V demo
    set(DEMO_NAME "nrf54l15_rv32_demo")
    add_executable(${DEMO_NAME} ${DEMO_SRC})
    
    # Set core-specific definitions
    target_compile_definitions(${DEMO_NAME} PRIVATE MICROPOSIX_CORE_RV32=1)
    
    # Include directories
    target_include_directories(${DEMO_NAME} PRIVATE
        ${MICROPOSIX_INCLUDE_DIRS}
        demo/nrf54l15
    )
    
    # Link with microPOSIX
    target_link_libraries(${DEMO_NAME} microposix)
    
    # Set output name
    set_target_properties(${DEMO_NAME} PROPERTIES
        OUTPUT_NAME "nrf54l15_rv32_demo"
        SUFFIX ".elf"
    )
endif()

# Conditional compilation for demo features
if(DEMO_IPC_TEST)
    target_compile_definitions(${DEMO_NAME} PRIVATE DEMO_IPC_TEST=1)
endif()

if(DEMO_THREAD_TEST)
    target_compile_definitions(${DEMO_NAME} PRIVATE DEMO_THREAD_TEST=1)
endif()

if(DEMO_SYNC_TEST)
    target_compile_definitions(${DEMO_NAME} PRIVATE DEMO_SYNC_TEST=1)
endif()
