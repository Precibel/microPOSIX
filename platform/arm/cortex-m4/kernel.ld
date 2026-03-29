/* Linker Script for microPOSIX Resident Kernel (Profile A - CC2755) */
MEMORY
{
    BOOT (rx)   : ORIGIN = 0x00000000, LENGTH = 32K
    FLASH (rx)  : ORIGIN = 0x00008000, LENGTH = 448K
    NVS (r)     : ORIGIN = 0x00078000, LENGTH = 32K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 160K
    SHARED (rw) : ORIGIN = 0x20028000, LENGTH = 32K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
    } > FLASH

    /* Corrected placement: End of OS FLASH (0x00078000 - 512 bytes) */
    .system_table 0x00077E00 :
    {
        KEEP(*(.system_table))
    } > FLASH

    .data :
    {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > RAM AT > FLASH

    .bss :
    {
        _sbss = .;
        *(.bss*)
        _ebss = .;
    } > RAM

    .stack :
    {
        . = ALIGN(8);
        _sstack = .;
        . = . + 8K; /* Kernel stack */
        _estack = .;
    } > RAM
}
