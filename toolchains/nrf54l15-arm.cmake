# nRF54L15 Toolchain File for Arm Cortex-M33
# 
# This file configures the toolchain for building the Cortex-M33 core
# on the nRF54L15 SoC.

# Set the system name
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain prefix
set(TOOLCHAIN_PREFIX arm-none-eabi-)

# Set the compiler and related tools
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)
set(CMAKE_NM ${TOOLCHAIN_PREFIX}nm)
set(CMAKE_STRIP ${TOOLCHAIN_PREFIX}strip)

# Set the find root path for the toolchain
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Set the sysroot
set(CMAKE_SYSROOT /usr/local/arm-none-eabi)

# Compiler flags
set(CMAKE_C_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -fdata-sections -ffunction-sections -fno-common -fno-builtin -mapcs-frame" CACHE INTERNAL "C compiler flags")
set(CMAKE_CXX_FLAGS ${CMAKE_C_FLAGS} CACHE INTERNAL "C++ compiler flags")
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -x assembler-with-cpp" CACHE INTERNAL "ASM compiler flags")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -specs=nosys.specs -specs=nano.specs -Wl,--gc-sections -Wl,--print-memory-usage" CACHE INTERNAL "Linker flags")

# Set the output format
set(CMAKE_EXECUTABLE_SUFFIX ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX ".elf")

# Set the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Set the output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add the toolchain include directories
include_directories(
    ${CMAKE_SYSROOT}/include
    ${CMAKE_SYSROOT}/include/c++/12.2.1
    ${CMAKE_SYSROOT}/include/c++/12.2.1/arm-none-eabi
    ${CMAKE_SYSROOT}/include/c++/12.2.1/backward
    ${CMAKE_SYSROOT}/lib/gcc/arm-none-eabi/12.2.1/include
)

# Set the compiler ID
set(CMAKE_COMPILER_IS_GNUCXX 1)
set(CMAKE_COMPILER_IS_GNUCC 1)

# Set the platform ID
set(CMAKE_PLATFORM_ID nRF54L15-M33)
