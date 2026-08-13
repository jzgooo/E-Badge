#include "badge.h"

#include "esp_log.h"

static const char *TAG = "badge";

esp_err_t badge_init(void)
{
    ESP_LOGI(TAG, "badge init, title=%s", badge_content_get_title());
    return ESP_OK;
}
