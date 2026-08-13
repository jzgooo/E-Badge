#include "audio.h"

#include "esp_log.h"

static const char *TAG = "audio";

esp_err_t audio_start(void)
{
    ESP_LOGI(TAG, "audio stub (ES8311/ES7210 not wired yet)");
    return ESP_OK;
}
