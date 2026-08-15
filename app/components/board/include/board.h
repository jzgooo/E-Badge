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

typedef struct {
    int percent;      /* 0–100；未知 -1 */
    int millivolts;   /* 电池电压 mV；无效 0 */
    bool charging;
    bool present;     /* 是否检测到电池 / 可读 */
} board_battery_t;

/**
 * 读 AXP2101 电量与电压。PMU 未初始化或失败时 percent=-1、millivolts=0、present=false。
 */
esp_err_t board_battery_get(board_battery_t *out);

#ifdef __cplusplus
}
#endif
