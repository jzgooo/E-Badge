#include "board.h"

#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"

static const char *TAG = "board";
static int s_brightness_percent = 70;

esp_err_t board_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs_flash_init failed");

    if (bsp_display_start() == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(bsp_display_backlight_on(), TAG, "backlight on failed");
    ESP_RETURN_ON_ERROR(board_brightness_set(70), TAG, "brightness failed");

    ESP_LOGI(TAG, "board ready");
    return ESP_OK;
}

bool board_display_lock(uint32_t timeout_ms)
{
    return bsp_display_lock(timeout_ms);
}

void board_display_unlock(void)
{
    bsp_display_unlock();
}

esp_err_t board_brightness_set(int percent)
{
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    esp_err_t err = bsp_display_brightness_set(percent);
    if (err == ESP_OK) {
        s_brightness_percent = percent;
    }
    return err;
}

int board_brightness_get(void)
{
    return s_brightness_percent;
}
