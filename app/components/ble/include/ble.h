#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Stable BLE API (maintained on main). GATT details belong in src/ble.c on feature branches. */
esp_err_t ble_start(void);

#ifdef __cplusplus
}
#endif
