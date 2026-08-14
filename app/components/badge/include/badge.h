#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Stable content / quota API (maintained on main signatures; quota helpers
 * added on the Codex feature branch).
 *
 * Stale rule (no RTC on 1.75C — option A):
 * - After reboot with NVS data and no new BLE write yet, treat as NOT stale
 *   for the first 24h of this boot (cannot prove wall-clock age).
 * - After a successful write this boot, stale when esp_timer exceeds 24h
 *   since that write.
 * - Empty (never synced / cleared) is not "stale"; UI shows the empty face.
 */
esp_err_t badge_init(void);

const char *badge_content_get_title(void);
const char *badge_content_get_subtitle(void);

#define BADGE_REMAIN_LABEL_MAX 16
#define BADGE_QUOTA_CAPTION_MAX 24
#define BADGE_REMAIN_PERCENT_UNKNOWN 255

typedef struct {
    uint8_t remain_percent; /* 0-100, or 255 = unknown */
    char remain_label[BADGE_REMAIN_LABEL_MAX + 1];
    char quota_caption[BADGE_QUOTA_CAPTION_MAX + 1];
    int64_t quota_updated_at; /* unix seconds; 0 = none */
} badge_quota_t;

typedef void (*badge_quota_changed_cb_t)(void);

void badge_quota_get(badge_quota_t *out);
bool badge_quota_is_empty(void);
bool badge_quota_is_stale(void);

esp_err_t badge_quota_apply_json(const char *json, size_t len);
esp_err_t badge_quota_to_json(char *buf, size_t buf_len, size_t *out_len);
esp_err_t badge_quota_clear(void);

void badge_quota_set_changed_cb(badge_quota_changed_cb_t cb);

#ifdef __cplusplus
}
#endif
