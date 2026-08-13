#include "ui.h"
#include "screens.h"
#include "board.h"

#include "esp_log.h"

static const char *TAG = "ui";

esp_err_t ui_start(void)
{
    if (!board_display_lock(1000)) {
        ESP_LOGE(TAG, "display lock timeout");
        return ESP_ERR_TIMEOUT;
    }

    screen_home_show();
    board_display_unlock();

    ESP_LOGI(TAG, "ui started");
    return ESP_OK;
}
