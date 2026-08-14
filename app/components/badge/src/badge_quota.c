#include "badge.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

static const char *TAG = "badge";

static const char *NVS_NS = "quota";
static const char *KEY_PERCENT = "pct";
static const char *KEY_LABEL = "label";
static const char *KEY_CAPTION = "caption";
static const char *KEY_UPDATED = "updated";

#define STALE_US (24LL * 60LL * 60LL * 1000000LL)

static badge_quota_t s_quota;
static int64_t s_boot_us;
static int64_t s_last_write_boot_us; /* 0 = no write this boot */
static badge_quota_changed_cb_t s_changed_cb;

static void notify_changed(void)
{
    if (s_changed_cb) {
        s_changed_cb();
    }
}

static void truncate_copy(char *dst, size_t dst_sz, const char *src, size_t max_chars)
{
    if (dst_sz == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t n = 0;
    while (src[n] != '\0' && n < max_chars && n + 1 < dst_sz) {
        n++;
    }
    while (n > 0 && (src[n] & 0xC0) == 0x80) {
        n--;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void reset_empty(void)
{
    memset(&s_quota, 0, sizeof(s_quota));
    s_quota.remain_percent = BADGE_REMAIN_PERCENT_UNKNOWN;
    s_last_write_boot_us = 0;
}

static esp_err_t nvs_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(h, KEY_PERCENT, s_quota.remain_percent);
    if (err == ESP_OK) {
        err = nvs_set_str(h, KEY_LABEL, s_quota.remain_label);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(h, KEY_CAPTION, s_quota.quota_caption);
    }
    if (err == ESP_OK) {
        err = nvs_set_i64(h, KEY_UPDATED, s_quota.quota_updated_at);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

static esp_err_t nvs_load(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        reset_empty();
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    badge_quota_t q;
    memset(&q, 0, sizeof(q));
    q.remain_percent = BADGE_REMAIN_PERCENT_UNKNOWN;

    err = nvs_get_u8(h, KEY_PERCENT, &q.remain_percent);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        reset_empty();
        return ESP_OK;
    }
    if (err != ESP_OK) {
        nvs_close(h);
        return err;
    }

    size_t len = sizeof(q.remain_label);
    err = nvs_get_str(h, KEY_LABEL, q.remain_label, &len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return err;
    }

    len = sizeof(q.quota_caption);
    err = nvs_get_str(h, KEY_CAPTION, q.quota_caption, &len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return err;
    }

    err = nvs_get_i64(h, KEY_UPDATED, &q.quota_updated_at);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        q.quota_updated_at = 0;
        err = ESP_OK;
    }
    nvs_close(h);
    if (err != ESP_OK) {
        return err;
    }

    s_quota = q;
    s_last_write_boot_us = 0;
    return ESP_OK;
}

static esp_err_t nvs_erase_namespace(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_all(h);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t badge_init(void)
{
    s_boot_us = esp_timer_get_time();
    reset_empty();
    esp_err_t err = nvs_load();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs load failed: %s — starting empty", esp_err_to_name(err));
        reset_empty();
    }
    ESP_LOGI(TAG, "badge init, empty=%d pct=%u updated=%lld", badge_quota_is_empty(),
             s_quota.remain_percent, (long long)s_quota.quota_updated_at);
    return ESP_OK;
}

void badge_quota_get(badge_quota_t *out)
{
    if (!out) {
        return;
    }
    *out = s_quota;
}

bool badge_quota_is_empty(void)
{
    return s_quota.quota_updated_at == 0 &&
           s_quota.remain_percent == BADGE_REMAIN_PERCENT_UNKNOWN &&
           s_quota.remain_label[0] == '\0';
}

bool badge_quota_is_stale(void)
{
    if (badge_quota_is_empty()) {
        return false;
    }
    const int64_t now = esp_timer_get_time();
    if (s_last_write_boot_us > 0) {
        return (now - s_last_write_boot_us) > STALE_US;
    }
    return (now - s_boot_us) > STALE_US;
}

esp_err_t badge_quota_apply_json(const char *json, size_t len)
{
    if (!json || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *jp = cJSON_GetObjectItemCaseSensitive(root, "remain_percent");
    const cJSON *jl = cJSON_GetObjectItemCaseSensitive(root, "remain_label");
    const cJSON *jc = cJSON_GetObjectItemCaseSensitive(root, "quota_caption");
    const cJSON *ju = cJSON_GetObjectItemCaseSensitive(root, "quota_updated_at");

    if (!cJSON_IsNumber(jp) || !cJSON_IsString(jl) || !cJSON_IsNumber(ju)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (jc && !cJSON_IsString(jc) && !cJSON_IsNull(jc)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    const int pct = jp->valueint;
    if (!((pct >= 0 && pct <= 100) || pct == BADGE_REMAIN_PERCENT_UNKNOWN)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    badge_quota_t next;
    memset(&next, 0, sizeof(next));
    next.remain_percent = (uint8_t)pct;
    truncate_copy(next.remain_label, sizeof(next.remain_label), jl->valuestring,
                  BADGE_REMAIN_LABEL_MAX);
    if (jc && cJSON_IsString(jc) && jc->valuestring) {
        truncate_copy(next.quota_caption, sizeof(next.quota_caption), jc->valuestring,
                      BADGE_QUOTA_CAPTION_MAX);
    }
    next.quota_updated_at = (int64_t)ju->valuedouble;

    cJSON_Delete(root);

    s_quota = next;
    s_last_write_boot_us = esp_timer_get_time();

    esp_err_t err = nvs_save();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs save failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "quota applied pct=%u label=%s caption=%s updated=%lld",
             s_quota.remain_percent, s_quota.remain_label, s_quota.quota_caption,
             (long long)s_quota.quota_updated_at);
    notify_changed();
    return ESP_OK;
}

esp_err_t badge_quota_to_json(char *buf, size_t buf_len, size_t *out_len)
{
    if (!buf || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddNumberToObject(root, "remain_percent", s_quota.remain_percent);
    cJSON_AddStringToObject(root, "remain_label", s_quota.remain_label);
    cJSON_AddStringToObject(root, "quota_caption", s_quota.quota_caption);
    cJSON_AddNumberToObject(root, "quota_updated_at", (double)s_quota.quota_updated_at);

    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) {
        return ESP_ERR_NO_MEM;
    }

    const size_t n = strlen(printed);
    if (n + 1 > buf_len) {
        cJSON_free(printed);
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(buf, printed, n + 1);
    cJSON_free(printed);
    if (out_len) {
        *out_len = n;
    }
    return ESP_OK;
}

esp_err_t badge_quota_clear(void)
{
    reset_empty();
    esp_err_t err = nvs_erase_namespace();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs erase failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "quota cleared");
    notify_changed();
    return ESP_OK;
}

void badge_quota_set_changed_cb(badge_quota_changed_cb_t cb)
{
    s_changed_cb = cb;
}
