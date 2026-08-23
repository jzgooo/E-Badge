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

/* Desktop dashboard protocol v1 (BLE characteristic 0xFF02). */
#define BADGE_DASHBOARD_MAX_CARDS 4
#define BADGE_CARD_LABEL_MAX BADGE_REMAIN_LABEL_MAX
#define BADGE_CARD_CAPTION_MAX BADGE_QUOTA_CAPTION_MAX
#define BADGE_CARD_PERCENT_UNKNOWN BADGE_REMAIN_PERCENT_UNKNOWN

typedef enum {
    BADGE_CARD_QUOTA = 0,
    BADGE_CARD_BUILD = 1,
    BADGE_CARD_FOCUS = 2,
    BADGE_CARD_SCHEDULE = 3,
    BADGE_CARD_INVALID = 255,
} badge_card_id_t;

typedef enum {
    BADGE_SEVERITY_NORMAL = 0,
    BADGE_SEVERITY_NEAR = 1,
    BADGE_SEVERITY_CRITICAL = 2,
} badge_severity_t;

typedef struct {
    badge_card_id_t id;
    badge_severity_t severity;
    uint8_t percent; /* 0-100, or 255 = no natural progress */
    char label[BADGE_CARD_LABEL_MAX + 1];
    char caption[BADGE_CARD_CAPTION_MAX + 1];
    int64_t updated_at; /* unix seconds supplied by the desktop */
    uint32_t ttl_sec;
    int64_t target_at; /* unix seconds; required by focus/schedule */
} badge_card_t;

typedef struct {
    uint8_t version;
    uint8_t card_count;
    badge_card_id_t source_default_card;
    badge_card_t cards[BADGE_DASHBOARD_MAX_CARDS];
} badge_dashboard_t;

typedef struct {
    bool enabled[BADGE_DASHBOARD_MAX_CARDS];
    badge_card_id_t order[BADGE_DASHBOARD_MAX_CARDS];
    badge_card_id_t default_override; /* BADGE_CARD_INVALID = use source default */
    bool sound_enabled; /* persisted preference; audio output is optional hardware work */
} badge_dashboard_prefs_t;

typedef void (*badge_dashboard_changed_cb_t)(void);

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

bool badge_dashboard_is_configured(void);
void badge_dashboard_get(badge_dashboard_t *out);
void badge_dashboard_get_prefs(badge_dashboard_prefs_t *out);
esp_err_t badge_dashboard_apply_json(const char *json, size_t len);
esp_err_t badge_dashboard_to_json(char *buf, size_t buf_len, size_t *out_len);
esp_err_t badge_dashboard_clear(void);
void badge_dashboard_set_changed_cb(badge_dashboard_changed_cb_t cb);

esp_err_t badge_dashboard_set_enabled(badge_card_id_t id, bool enabled);
esp_err_t badge_dashboard_move_card(badge_card_id_t id, int direction);
esp_err_t badge_dashboard_set_default_override(badge_card_id_t id);
esp_err_t badge_dashboard_set_sound_enabled(bool enabled);

bool badge_dashboard_card_is_stale(const badge_card_t *card);
bool badge_dashboard_card_needs_sync(const badge_card_t *card);
int badge_dashboard_card_remaining_sec(const badge_card_t *card);
const badge_card_t *badge_dashboard_find_card(badge_card_id_t id);
const badge_card_t *badge_dashboard_select_priority(void);
const char *badge_card_id_name(badge_card_id_t id);

#ifdef __cplusplus
}
#endif
