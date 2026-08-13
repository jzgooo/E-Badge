#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize NVS, display, and board-level peripherals. */
esp_err_t board_init(void);

bool board_display_lock(uint32_t timeout_ms);
void board_display_unlock(void);

#ifdef __cplusplus
}
#endif
