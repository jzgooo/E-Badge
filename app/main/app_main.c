#include "esp_err.h"
#include "esp_log.h"

#include "board.h"
#include "badge.h"
#include "ui.h"
#include "sensors.h"
#include "audio.h"
#include "ble.h"

static const char *TAG = "app";

void app_main(void)
{
    ESP_LOGI(TAG, "Codex quota starting");

    ESP_ERROR_CHECK(board_init());
    ESP_ERROR_CHECK(badge_init());
    ESP_ERROR_CHECK(ui_start());
    ESP_ERROR_CHECK(sensors_start());
    ESP_ERROR_CHECK(audio_start());
    ESP_ERROR_CHECK(ble_start());

    ESP_LOGI(TAG, "Codex quota running");
}
