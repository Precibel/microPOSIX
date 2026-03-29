/* Linker Script for User Application (Profile A - Slot 0) */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00080000, LENGTH = 240K
    RAM (rwx)   : ORIGIN = 0x20030000, LENGTH = 64K
}

SECTIONS
{
    .text :
    {
        /* Entry point header at the very beginning of the App flash */
        KEEP(*(.app_header))
        *(.text*)
        *(.rodata*)
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
}
