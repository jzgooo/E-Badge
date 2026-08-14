#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Stable board API (maintained on main).
 * Downstream branches should keep these signatures and extend src/board.c.
 */
esp_err_t board_init(void);

bool board_display_lock(uint32_t timeout_ms);
void board_display_unlock(void);

/** Brightness 0-100. M1: settings can change; sleep/AXP is M2. */
esp_err_t board_brightness_set(int percent);
int board_brightness_get(void);

#ifdef __cplusplus
}
#endif
