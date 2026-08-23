#include "badge.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

static const char *TAG = "dashboard";
static const char *NVS_NS = "dashboard";
static const char *KEY_SNAPSHOT = "snapshot";
static const char *KEY_PREFS = "prefs";

#define DASHBOARD_VERSION 1
#define MAX_TTL_SEC (7U * 24U * 60U * 60U)

static badge_dashboard_t s_dashboard;
static badge_dashboard_prefs_t s_prefs;
static bool s_configured;
static int64_t s_boot_us;
static int64_t s_last_write_boot_us;
static badge_dashboard_changed_cb_t s_changed_cb;

static int64_t dashboard_reference_updated_at(void)
{
    int64_t newest = 0;
    for (int i = 0; i < s_dashboard.card_count; ++i) {
        if (s_dashboard.cards[i].updated_at > newest) newest = s_dashboard.cards[i].updated_at;
    }
    return newest;
}

static bool card_id_valid(badge_card_id_t id)
{
    return id >= BADGE_CARD_QUOTA && id <= BADGE_CARD_SCHEDULE;
}

const char *badge_card_id_name(badge_card_id_t id)
{
    switch (id) {
    case BADGE_CARD_QUOTA: return "quota";
    case BADGE_CARD_BUILD: return "build";
    case BADGE_CARD_FOCUS: return "focus";
    case BADGE_CARD_SCHEDULE: return "schedule";
    default: return NULL;
    }
}

static badge_card_id_t card_id_from_name(const char *name)
{
    for (int i = BADGE_CARD_QUOTA; i <= BADGE_CARD_SCHEDULE; ++i) {
        const badge_card_id_t id = (badge_card_id_t)i;
        const char *candidate = badge_card_id_name(id);
        if (candidate && name && strcmp(candidate, name) == 0) {
            return id;
        }
    }
    return BADGE_CARD_INVALID;
}

static const char *severity_name(badge_severity_t severity)
{
    switch (severity) {
    case BADGE_SEVERITY_NORMAL: return "normal";
    case BADGE_SEVERITY_NEAR: return "near";
    case BADGE_SEVERITY_CRITICAL: return "critical";
    default: return NULL;
    }
}

static bool severity_from_name(const char *name, badge_severity_t *out)
{
    if (!name || !out) return false;
    if (strcmp(name, "normal") == 0) *out = BADGE_SEVERITY_NORMAL;
    else if (strcmp(name, "near") == 0) *out = BADGE_SEVERITY_NEAR;
    else if (strcmp(name, "critical") == 0) *out = BADGE_SEVERITY_CRITICAL;
    else return false;
    return true;
}

static bool utf8_copy_checked(char *dst, size_t dst_size, const char *src, size_t max_bytes)
{
    if (!dst || dst_size == 0 || !src) return false;
    const size_t len = strlen(src);
    if (len > max_bytes || len + 1 > dst_size) return false;
    for (size_t i = 0; i < len; ++i) {
        const unsigned char c = (unsigned char)src[i];
        if (c < 0x80) continue;
        size_t tail = 0;
        if ((c & 0xE0) == 0xC0) tail = 1;
        else if ((c & 0xF0) == 0xE0) tail = 2;
        else if ((c & 0xF8) == 0xF0) tail = 3;
        else return false;
        if (i + tail >= len) return false;
        for (size_t j = 1; j <= tail; ++j) {
            if (((unsigned char)src[i + j] & 0xC0) != 0x80) return false;
        }
        i += tail;
    }
    memcpy(dst, src, len + 1);
    return true;
}

static void reset_prefs(void)
{
    memset(&s_prefs, 0, sizeof(s_prefs));
    for (int i = 0; i < BADGE_DASHBOARD_MAX_CARDS; ++i) {
        s_prefs.enabled[i] = true;
        s_prefs.order[i] = (badge_card_id_t)i;
    }
    s_prefs.default_override = BADGE_CARD_INVALID;
    s_prefs.sound_enabled = true;
}

static void reset_dashboard(void)
{
    memset(&s_dashboard, 0, sizeof(s_dashboard));
    s_dashboard.version = DASHBOARD_VERSION;
    s_dashboard.source_default_card = BADGE_CARD_INVALID;
    s_configured = false;
    s_last_write_boot_us = 0;
}

static void notify_changed(void)
{
    if (s_changed_cb) s_changed_cb();
}

static esp_err_t nvs_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, KEY_SNAPSHOT, &s_dashboard, sizeof(s_dashboard));
    if (err == ESP_OK) err = nvs_set_blob(h, KEY_PREFS, &s_prefs, sizeof(s_prefs));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t nvs_save_prefs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, KEY_PREFS, &s_prefs, sizeof(s_prefs));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static bool dashboard_valid(const badge_dashboard_t *dashboard)
{
    if (!dashboard || dashboard->version != DASHBOARD_VERSION ||
        dashboard->card_count == 0 || dashboard->card_count > BADGE_DASHBOARD_MAX_CARDS ||
        !card_id_valid(dashboard->source_default_card)) return false;
    bool seen[BADGE_DASHBOARD_MAX_CARDS] = {false};
    bool has_default = false;
    for (int i = 0; i < dashboard->card_count; ++i) {
        const badge_card_t *card = &dashboard->cards[i];
        if (!card_id_valid(card->id) || seen[card->id] ||
            (card->percent != BADGE_CARD_PERCENT_UNKNOWN && card->percent > 100) ||
            card->severity > BADGE_SEVERITY_CRITICAL || card->ttl_sec == 0 ||
            card->ttl_sec > MAX_TTL_SEC || card->updated_at <= 0 ||
            card->label[0] == '\0') return false;
        if ((card->id == BADGE_CARD_FOCUS || card->id == BADGE_CARD_SCHEDULE) &&
            card->target_at <= card->updated_at) return false;
        seen[card->id] = true;
        if (card->id == dashboard->source_default_card) has_default = true;
    }
    return has_default;
}

static esp_err_t nvs_load(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    size_t snapshot_len = sizeof(s_dashboard);
    err = nvs_get_blob(h, KEY_SNAPSHOT, &s_dashboard, &snapshot_len);
    if (err != ESP_OK || snapshot_len != sizeof(s_dashboard) || !dashboard_valid(&s_dashboard)) {
        nvs_close(h);
        reset_dashboard();
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : ESP_FAIL;
    }
    size_t prefs_len = sizeof(s_prefs);
    err = nvs_get_blob(h, KEY_PREFS, &s_prefs, &prefs_len);
    nvs_close(h);
    if (err != ESP_OK || prefs_len != sizeof(s_prefs)) reset_prefs();
    s_configured = true;
    return ESP_OK;
}

static esp_err_t nvs_erase(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    err = nvs_erase_all(h);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static bool parse_card(const cJSON *item, badge_card_t *out)
{
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
    const cJSON *percent = cJSON_GetObjectItemCaseSensitive(item, "percent");
    const cJSON *label = cJSON_GetObjectItemCaseSensitive(item, "label");
    const cJSON *caption = cJSON_GetObjectItemCaseSensitive(item, "caption");
    const cJSON *updated = cJSON_GetObjectItemCaseSensitive(item, "updated_at");
    const cJSON *ttl = cJSON_GetObjectItemCaseSensitive(item, "ttl_sec");
    const cJSON *severity = cJSON_GetObjectItemCaseSensitive(item, "severity");
    const cJSON *target = cJSON_GetObjectItemCaseSensitive(item, "target_at");
    if (!cJSON_IsString(id) || !cJSON_IsNumber(percent) || !cJSON_IsString(label) ||
        !cJSON_IsNumber(updated) || !cJSON_IsNumber(ttl) || !cJSON_IsString(severity)) return false;
    memset(out, 0, sizeof(*out));
    out->id = card_id_from_name(id->valuestring);
    const int pct = percent->valueint;
    if (!card_id_valid(out->id) || !((pct >= 0 && pct <= 100) || pct == BADGE_CARD_PERCENT_UNKNOWN) ||
        updated->valuedouble <= 0 || ttl->valuedouble <= 0 || ttl->valuedouble > MAX_TTL_SEC ||
        !severity_from_name(severity->valuestring, &out->severity) ||
        !utf8_copy_checked(out->label, sizeof(out->label), label->valuestring, BADGE_CARD_LABEL_MAX)) return false;
    if (caption && !cJSON_IsNull(caption)) {
        if (!cJSON_IsString(caption) || !utf8_copy_checked(out->caption, sizeof(out->caption),
                                                            caption->valuestring, BADGE_CARD_CAPTION_MAX)) return false;
    }
    out->percent = (uint8_t)pct;
    out->updated_at = (int64_t)updated->valuedouble;
    out->ttl_sec = (uint32_t)ttl->valuedouble;
    if (target && !cJSON_IsNull(target)) {
        if (!cJSON_IsNumber(target)) return false;
        out->target_at = (int64_t)target->valuedouble;
    }
    return true;
}

esp_err_t badge_dashboard_apply_json(const char *json, size_t len)
{
    if (!json || len == 0) return ESP_ERR_INVALID_ARG;
    cJSON *root = cJSON_ParseWithLength(json, len);
    const cJSON *version = root ? cJSON_GetObjectItemCaseSensitive(root, "version") : NULL;
    const cJSON *default_card = root ? cJSON_GetObjectItemCaseSensitive(root, "default_card") : NULL;
    const cJSON *cards = root ? cJSON_GetObjectItemCaseSensitive(root, "cards") : NULL;
    if (!root || !cJSON_IsObject(root) || !cJSON_IsNumber(version) || version->valueint != DASHBOARD_VERSION ||
        !cJSON_IsString(default_card) || !cJSON_IsArray(cards)) {
        cJSON_Delete(root); return ESP_ERR_INVALID_ARG;
    }
    const int count = cJSON_GetArraySize(cards);
    if (count < 1 || count > BADGE_DASHBOARD_MAX_CARDS) {
        cJSON_Delete(root); return ESP_ERR_INVALID_SIZE;
    }
    badge_dashboard_t next;
    memset(&next, 0, sizeof(next));
    next.version = DASHBOARD_VERSION;
    next.card_count = (uint8_t)count;
    next.source_default_card = card_id_from_name(default_card->valuestring);
    for (int i = 0; i < count; ++i) {
        if (!parse_card(cJSON_GetArrayItem(cards, i), &next.cards[i])) {
            cJSON_Delete(root); return ESP_ERR_INVALID_ARG;
        }
    }
    cJSON_Delete(root);
    if (!dashboard_valid(&next)) return ESP_ERR_INVALID_ARG;
    const badge_dashboard_t previous = s_dashboard;
    const bool previous_configured = s_configured;
    const int64_t previous_write_boot_us = s_last_write_boot_us;
    s_dashboard = next;
    s_configured = true;
    s_last_write_boot_us = esp_timer_get_time();
    esp_err_t err = nvs_save();
    if (err != ESP_OK) {
        s_dashboard = previous;
        s_configured = previous_configured;
        s_last_write_boot_us = previous_write_boot_us;
        return err;
    }
    ESP_LOGI(TAG, "dashboard snapshot applied cards=%u", s_dashboard.card_count);
    notify_changed();
    return ESP_OK;
}

static cJSON *card_to_json(const badge_card_t *card)
{
    cJSON *item = cJSON_CreateObject();
    if (!item) return NULL;
    cJSON_AddStringToObject(item, "id", badge_card_id_name(card->id));
    cJSON_AddNumberToObject(item, "percent", card->percent);
    cJSON_AddStringToObject(item, "label", card->label);
    cJSON_AddStringToObject(item, "caption", card->caption);
    cJSON_AddNumberToObject(item, "updated_at", (double)card->updated_at);
    cJSON_AddNumberToObject(item, "ttl_sec", card->ttl_sec);
    cJSON_AddStringToObject(item, "severity", severity_name(card->severity));
    if (card->target_at > 0) cJSON_AddNumberToObject(item, "target_at", (double)card->target_at);
    return item;
}

esp_err_t badge_dashboard_to_json(char *buf, size_t buf_len, size_t *out_len)
{
    if (!buf || buf_len == 0) return ESP_ERR_INVALID_ARG;
    cJSON *root = cJSON_CreateObject();
    cJSON *cards = cJSON_CreateArray();
    if (!root || !cards) { cJSON_Delete(root); cJSON_Delete(cards); return ESP_ERR_NO_MEM; }
    cJSON_AddNumberToObject(root, "version", DASHBOARD_VERSION);
    cJSON_AddStringToObject(root, "default_card", badge_card_id_name(s_dashboard.source_default_card) ?: "");
    cJSON_AddItemToObject(root, "cards", cards);
    for (int i = 0; i < s_dashboard.card_count; ++i) {
        cJSON *item = card_to_json(&s_dashboard.cards[i]);
        if (!item) { cJSON_Delete(root); return ESP_ERR_NO_MEM; }
        cJSON_AddItemToArray(cards, item);
    }
    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) return ESP_ERR_NO_MEM;
    const size_t length = strlen(printed);
    if (length + 1 > buf_len) { cJSON_free(printed); return ESP_ERR_INVALID_SIZE; }
    memcpy(buf, printed, length + 1);
    cJSON_free(printed);
    if (out_len) *out_len = length;
    return ESP_OK;
}

bool badge_dashboard_is_configured(void) { return s_configured; }
void badge_dashboard_get(badge_dashboard_t *out) { if (out) *out = s_dashboard; }
void badge_dashboard_get_prefs(badge_dashboard_prefs_t *out) { if (out) *out = s_prefs; }
void badge_dashboard_set_changed_cb(badge_dashboard_changed_cb_t cb) { s_changed_cb = cb; }

const badge_card_t *badge_dashboard_find_card(badge_card_id_t id)
{
    if (!s_configured) return NULL;
    for (int i = 0; i < s_dashboard.card_count; ++i) {
        if (s_dashboard.cards[i].id == id) return &s_dashboard.cards[i];
    }
    return NULL;
}

bool badge_dashboard_card_needs_sync(const badge_card_t *card)
{
    return card && s_last_write_boot_us == 0 &&
        (card->id == BADGE_CARD_FOCUS || card->id == BADGE_CARD_SCHEDULE);
}

bool badge_dashboard_card_is_stale(const badge_card_t *card)
{
    if (!card) return true;
    if (badge_dashboard_card_needs_sync(card)) return true;
    const int64_t origin = s_last_write_boot_us > 0 ? s_last_write_boot_us : s_boot_us;
    const int64_t elapsed_sec = (esp_timer_get_time() - origin) / 1000000LL;
    const int64_t source_age_sec = dashboard_reference_updated_at() - card->updated_at;
    return source_age_sec + elapsed_sec > card->ttl_sec;
}

int badge_dashboard_card_remaining_sec(const badge_card_t *card)
{
    if (!card || card->target_at <= 0 || badge_dashboard_card_needs_sync(card)) return -1;
    const int64_t origin = s_last_write_boot_us > 0 ? s_last_write_boot_us : s_boot_us;
    const int64_t elapsed = (esp_timer_get_time() - origin) / 1000000LL;
    const int64_t remaining = card->target_at - dashboard_reference_updated_at() - elapsed;
    return remaining <= 0 ? 0 : (remaining > INT32_MAX ? INT32_MAX : (int)remaining);
}

static bool card_enabled(badge_card_id_t id)
{
    return card_id_valid(id) && s_prefs.enabled[id] && badge_dashboard_find_card(id) != NULL;
}

const badge_card_t *badge_dashboard_select_priority(void)
{
    if (!s_configured) return NULL;
    for (int i = 0; i < s_dashboard.card_count; ++i) {
        const badge_card_t *card = &s_dashboard.cards[i];
        if (card_enabled(card->id) && !badge_dashboard_card_is_stale(card) &&
            card->severity == BADGE_SEVERITY_CRITICAL) return card;
    }
    const badge_card_id_t near_order[] = {BADGE_CARD_SCHEDULE, BADGE_CARD_FOCUS};
    for (size_t i = 0; i < sizeof(near_order) / sizeof(near_order[0]); ++i) {
        const badge_card_t *card = badge_dashboard_find_card(near_order[i]);
        const int remaining = badge_dashboard_card_remaining_sec(card);
        if (card_enabled(near_order[i]) && !badge_dashboard_card_is_stale(card) &&
            (card->severity == BADGE_SEVERITY_NEAR ||
             (card->id == BADGE_CARD_SCHEDULE && remaining >= 0 && remaining <= 600) ||
             (card->id == BADGE_CARD_FOCUS && remaining >= 0 && remaining <= 120))) return card;
    }
    const badge_card_id_t preferred = card_id_valid(s_prefs.default_override) ?
        s_prefs.default_override : s_dashboard.source_default_card;
    if (card_enabled(preferred)) return badge_dashboard_find_card(preferred);
    for (int i = 0; i < BADGE_DASHBOARD_MAX_CARDS; ++i) {
        if (card_enabled(s_prefs.order[i])) return badge_dashboard_find_card(s_prefs.order[i]);
    }
    return NULL;
}

static esp_err_t save_prefs_and_notify(void)
{
    esp_err_t err = nvs_save_prefs();
    if (err == ESP_OK) notify_changed();
    return err;
}

esp_err_t badge_dashboard_set_enabled(badge_card_id_t id, bool enabled)
{
    if (!card_id_valid(id)) return ESP_ERR_INVALID_ARG;
    s_prefs.enabled[id] = enabled;
    return save_prefs_and_notify();
}

esp_err_t badge_dashboard_move_card(badge_card_id_t id, int direction)
{
    if (!card_id_valid(id) || (direction != -1 && direction != 1)) return ESP_ERR_INVALID_ARG;
    for (int i = 0; i < BADGE_DASHBOARD_MAX_CARDS; ++i) {
        if (s_prefs.order[i] == id) {
            const int other = i + direction;
            if (other < 0 || other >= BADGE_DASHBOARD_MAX_CARDS) return ESP_OK;
            const badge_card_id_t swap = s_prefs.order[other];
            s_prefs.order[other] = id;
            s_prefs.order[i] = swap;
            return save_prefs_and_notify();
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t badge_dashboard_set_default_override(badge_card_id_t id)
{
    if (!card_id_valid(id)) return ESP_ERR_INVALID_ARG;
    s_prefs.default_override = id;
    return save_prefs_and_notify();
}

esp_err_t badge_dashboard_set_sound_enabled(bool enabled)
{
    s_prefs.sound_enabled = enabled;
    return save_prefs_and_notify();
}

esp_err_t badge_dashboard_clear(void)
{
    esp_err_t err = nvs_erase();
    if (err != ESP_OK) return err;
    reset_dashboard();
    reset_prefs();
    notify_changed();
    return ESP_OK;
}

/* Called from badge_init after NVS is available. */
esp_err_t badge_dashboard_init(void)
{
    s_boot_us = esp_timer_get_time();
    reset_dashboard();
    reset_prefs();
    esp_err_t err = nvs_load();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "dashboard NVS load failed: %s", esp_err_to_name(err));
        reset_dashboard();
        reset_prefs();
    }
    return ESP_OK;
}

/* Legacy 0xFF01 quota writes update the quota card when dashboard mode is active. */
esp_err_t badge_dashboard_apply_legacy_quota(const badge_quota_t *quota)
{
    if (!s_configured || !quota) return ESP_OK;
    const badge_dashboard_t previous = s_dashboard;
    const int64_t previous_write_boot_us = s_last_write_boot_us;
    badge_card_t *target = NULL;
    for (int i = 0; i < s_dashboard.card_count; ++i) {
        if (s_dashboard.cards[i].id == BADGE_CARD_QUOTA) target = &s_dashboard.cards[i];
    }
    if (!target) {
        if (s_dashboard.card_count >= BADGE_DASHBOARD_MAX_CARDS) return ESP_ERR_NO_MEM;
        target = &s_dashboard.cards[s_dashboard.card_count++];
        memset(target, 0, sizeof(*target));
        target->id = BADGE_CARD_QUOTA;
        target->ttl_sec = 86400;
        target->severity = BADGE_SEVERITY_NORMAL;
    }
    target->percent = quota->remain_percent;
    memcpy(target->label, quota->remain_label, sizeof(target->label));
    memcpy(target->caption, quota->quota_caption, sizeof(target->caption));
    target->updated_at = quota->quota_updated_at;
    target->target_at = 0;
    s_last_write_boot_us = esp_timer_get_time();
    esp_err_t err = nvs_save();
    if (err == ESP_OK) {
        notify_changed();
    } else {
        s_dashboard = previous;
        s_last_write_boot_us = previous_write_boot_us;
    }
    return err;
}
