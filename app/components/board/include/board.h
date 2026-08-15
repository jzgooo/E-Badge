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

/** User brightness preference 10–100; persisted. Effective level may be lower when battery is low. */
esp_err_t board_brightness_set(int percent);
int board_brightness_get(void);
/** Recompute panel brightness from preference + low-battery / sleep policy. */
void board_brightness_reapply(void);

esp_err_t board_display_sleep(void);
esp_err_t board_display_wake(void);
bool board_display_is_asleep(void);

/** Auto-sleep timeout: only 5 / 10 / 30 seconds (default 10). Persisted. */
int board_sleep_timeout_sec_get(void);
esp_err_t board_sleep_timeout_sec_set(int sec);

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

typedef enum {
    BOARD_PWR_EVENT_SHORT = 0, /* 亮灭屏 */
    BOARD_PWR_EVENT_LONG = 1,  /* 关机 */
} board_pwr_event_t;

typedef void (*board_pwr_event_cb_t)(board_pwr_event_t event);

void board_pwr_set_event_cb(board_pwr_event_cb_t cb);

/** AXP2101 关断全路电源。 */
esp_err_t board_shutdown(void);

#ifdef __cplusplus
}
#endif
