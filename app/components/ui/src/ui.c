#include "ui.h"
#include "screens.h"
#include "board.h"
#include "badge.h"

#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ui";

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

esp_err_t ui_start(void)
{
    badge_quota_set_changed_cb(on_quota_changed);

    if (!board_display_lock(1000)) {
        ESP_LOGE(TAG, "display lock timeout");
        return ESP_ERR_TIMEOUT;
    }

    screen_home_show();
    board_display_unlock();

    ESP_LOGI(TAG, "ui started");
    return ESP_OK;
}
