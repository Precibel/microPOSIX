/**
 * @file linker.ld
 * @brief Linker Script for Cortex-M33 (nRF54L15)
 * 
 * This linker script defines the memory layout for the Cortex-M33 core
 * on the nRF54L15 SoC.
 */

/*
 * Memory Regions for nRF54L15
 * 
 * Note: These addresses are based on the nRF54L15 datasheet.
 * Adjust them according to the actual memory map of your specific device.
 */

MEMORY
{
    /* Flash Memory */
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 2M
    
    /* SRAM - Core-specific */
    SRAM_M33 (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
    
    /* Shared SRAM (accessible by both cores) */
    SHARED_SRAM (rwx) : ORIGIN = 0x20040000, LENGTH = 64K
}

/*
 * Entry Point
 */

ENTRY(Reset_Handler)

/*
 * Sections
 */

SECTIONS
{
    /*
     * .text Section - Code
     */
    .text :
    {
        *(.vectors)           /* Exception vector table */
        *(.text.startup)      /* Startup code */
        *(.text.trap_vector)  /* Trap vector */
        *(.text.trap_handlers)/* Trap handlers */
        *(.text.pendsv)       /* PendSV handler */
        *(.text*)             /* All other code */
        *(.rodata*)           /* Read-only data */
        *(.rodata.*)         /* Read-only data (subsections) */
    } > FLASH
    
    /*
     * .data Section - Initialized Data
     */
    .data :
    {
        *(.data*)             /* All initialized data */
        *(.data.*)            /* Initialized data (subsections) */
    } > SRAM_M33 AT > FLASH
    
    /*
     * .bss Section - Zero-initialized Data
     */
    .bss :
    {
        *(.bss*)              /* All zero-initialized data */
        *(.bss.*)             /* Zero-initialized data (subsections) */
        *(COMMON)             /* Common symbols */
    } > SRAM_M33
    
    /*
     * .stack Section - Stack
     */
    .stack :
    {
        . = ALIGN(8);
        __stack_start__ = .;
        *(.stack*)
        . = ALIGN(8);
        __stack_top = .;
    } > SRAM_M33
    
    /*
     * .heap Section - Heap
     */
    .heap :
    {
        . = ALIGN(8);
        __heap_start__ = .;
        *(.heap*)
        . = ALIGN(8);
        __heap_end__ = .;
    } > SRAM_M33
    
    /*
     * .shared_ram Section - Shared Memory
     * 
     * This section is for data that needs to be shared between cores.
     */
    .shared_ram :
    {
        . = ALIGN(8);
        *(.shared_ram*)
    } > SHARED_SRAM
    
    /*
     * .noinit Section - Uninitialized Data (retained across resets)
     */
    .noinit :
    {
        *(.noinit*)
    } > SRAM_M33
    
    /*
     * ARM Attributes
     */
    .ARM.attributes : { *(.ARM.attributes) } > FLASH
    
    /*
     * Debug Sections
     */
    .debug : { *(.debug*) } > FLASH
    .line : { *(.line*) } > FLASH
    .loc : { *(.loc*) } > FLASH
    .stab : { *(.stab*) } > FLASH
    .stabstr : { *(.stabstr*) } > FLASH
}

/*
 * Symbol Definitions
 */

PROVIDE(__stack_start = ORIGIN(SRAM_M33) + LENGTH(SRAM_M33) - 8);
PROVIDE(__stack_top = ORIGIN(SRAM_M33) + LENGTH(SRAM_M33));
PROVIDE(__heap_start = __stack_start__);
PROVIDE(__heap_end = ORIGIN(SRAM_M33) + LENGTH(SRAM_M33));

/*
 * Data Section Boundaries for Startup Code
 */

PROVIDE(__data_start__ = ADDR(.data));
PROVIDE(__data_end__ = ADDR(.data) + SIZEOF(.data));
PROVIDE(__data_load__ = LOADADDR(.data));

PROVIDE(__bss_start__ = ADDR(.bss));
PROVIDE(__bss_end__ = ADDR(.bss) + SIZEOF(.bss));
