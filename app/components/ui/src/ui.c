#include "ui.h"
#include "screens.h"
#include "board.h"
#include "badge.h"

#include "esp_log.h"
#include "lvgl.h"

#include <stdint.h>

static const char *TAG = "ui";

static lv_timer_t *s_power_timer;
static uint32_t s_inactive_at_sleep;
static bool s_track_sleep_inactive;

static void on_quota_changed_async(void *user_data)
{
    LV_UNUSED(user_data);
    screen_home_refresh();
}

static void on_quota_changed(void)
{
    /* BLE host task → LVGL thread via async call. */
    lv_async_call(on_quota_changed_async, NULL);
}

static void on_dashboard_changed(void)
{
    /* Dashboard writes use the same BLE host task as legacy quota writes. */
    lv_async_call(on_quota_changed_async, NULL);
}

static void wake_from_ui(void)
{
    board_display_wake();
    s_track_sleep_inactive = false;
    lv_display_trigger_activity(NULL);
}

static void sleep_from_ui(void)
{
    s_inactive_at_sleep = lv_display_get_inactive_time(NULL);
    s_track_sleep_inactive = true;
    board_display_sleep();
}

static void on_pwr_event_async(void *user_data)
{
    board_pwr_event_t ev = (board_pwr_event_t)(uintptr_t)user_data;
    if (ev == BOARD_PWR_EVENT_LONG) {
        board_shutdown();
        return;
    }

    /* 息屏：短按只亮屏。亮屏主页：短按进设置。灭屏靠超时。 */
    if (board_display_is_asleep()) {
        wake_from_ui();
    } else if (screen_home_is_active()) {
        lv_display_trigger_activity(NULL);
        screen_open_settings();
    }
}

static void on_pwr_event(board_pwr_event_t event)
{
    lv_async_call(on_pwr_event_async, (void *)(uintptr_t)event);
}

static void on_power_timer(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    board_brightness_reapply();
    screen_home_refresh();

    const uint32_t inactive_ms = lv_display_get_inactive_time(NULL);
    const uint32_t timeout_ms = (uint32_t)board_sleep_timeout_sec_get() * 1000U;

    if (board_display_is_asleep()) {
        if (s_track_sleep_inactive && inactive_ms < s_inactive_at_sleep) {
            wake_from_ui();
        }
        return;
    }

    if (inactive_ms >= timeout_ms) {
        sleep_from_ui();
    }
}

esp_err_t ui_start(void)
{
    badge_quota_set_changed_cb(on_quota_changed);
    badge_dashboard_set_changed_cb(on_dashboard_changed);
    board_pwr_set_event_cb(on_pwr_event);

    if (!board_display_lock(1000)) {
        ESP_LOGE(TAG, "display lock timeout");
        return ESP_ERR_TIMEOUT;
    }

    screen_home_show();
    s_power_timer = lv_timer_create(on_power_timer, 250, NULL);
    board_display_unlock();

    ESP_LOGI(TAG, "ui started (M2 sleep/pwr)");
    return ESP_OK;
}
