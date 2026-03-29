#ifndef MICROPOSIX_BOOT_H
#define MICROPOSIX_BOOT_H

#include <stdint.h>
#include <stdbool.h>

// Flash layout (Example for nRF52840 or STM32)
#define FLASH_BASE           0x00000000
#define BOOTLOADER_SIZE      0x00008000 // 32KB
#define SLOT_A_START         (FLASH_BASE + BOOTLOADER_SIZE)
#define SLOT_SIZE            0x00078000 // 480KB each
#define SLOT_B_START         (SLOT_A_START + SLOT_SIZE)

typedef enum {
    BOOT_SLOT_A,
    BOOT_SLOT_B,
    BOOT_UART_UPDATE
} mp_boot_mode_t;

typedef struct {
    uint32_t magic;      // 0x55AA55AA
    uint32_t version;
    uint32_t image_size;
    uint32_t checksum;
    uint8_t  active_slot; // 0 for A, 1 for B
} mp_boot_info_t;

// Bootloader functions
void mp_boot_init(void);
void mp_boot_jump_to_app(uint32_t app_address);
bool mp_boot_authenticate_uart(void);
void mp_boot_uart_update(void);

#endif // MICROPOSIX_BOOT_H
