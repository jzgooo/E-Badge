#include "sensors.h"

#include "esp_log.h"

static const char *TAG = "sensors";

esp_err_t sensors_start(void)
{
    ESP_LOGI(TAG, "sensors stub (QMI8658 not wired yet)");
    return ESP_OK;
}
