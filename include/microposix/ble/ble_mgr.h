#ifndef MICROPOSIX_BLE_MGR_H
#define MICROPOSIX_BLE_MGR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MP_BLE_ADVERTISING,
    MP_BLE_CONNECTED,
    MP_BLE_DISCONNECTED,
    MP_BLE_ERROR
} mp_ble_state_t;

void mp_ble_init(void);
void mp_ble_start_adv(void);
void mp_ble_stop_adv(void);
mp_ble_state_t mp_ble_get_state(void);

#endif // MICROPOSIX_BLE_MGR_H
