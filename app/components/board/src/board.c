#include "board.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "lvgl.h"

static const char *TAG = "board";
static int s_brightness_percent = 70;
static lv_display_t *s_disp;

/* ESP32-S3 数据缓存行 64 字节，PSRAM DMA 需要按此对齐。 */
#define BOARD_FB_ALIGN 64

/*
 * BSP 默认用 50 行条带缓冲：SPI bounce 能进内部 RAM，但 466x466 会条带闪屏。
 * 显示启动后再换成双全帧 PSRAM 画布（466*466*2 ≈ 434KB/块）。
 * SPI 仍按 32KB 分段发送（见 BSP max_transfer_sz），不要把全帧放进内部 RAM。
 */
static esp_err_t board_display_install_full_buffers(void)
{
    const uint32_t w = lv_display_get_horizontal_resolution(s_disp);
    const uint32_t h = lv_display_get_vertical_resolution(s_disp);
    const uint32_t bpp = lv_color_format_get_size(lv_display_get_color_format(s_disp));
    const uint32_t buf_size = w * h * bpp;

    void *buf1 = heap_caps_aligned_alloc(BOARD_FB_ALIGN, buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *buf2 = heap_caps_aligned_alloc(BOARD_FB_ALIGN, buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf1 || !buf2) {
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        ESP_LOGE(TAG, "full framebuffer alloc failed (%lu bytes each)", (unsigned long)buf_size);
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_buffers(s_disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    ESP_LOGI(TAG, "PSRAM full buffers installed (%ux%u, %lu bytes x2)",
             (unsigned)w, (unsigned)h, (unsigned long)buf_size);
    return ESP_OK;
}

esp_err_t board_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs_flash_init failed");

    s_disp = bsp_display_start();
    if (s_disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return ESP_FAIL;
    }

    /* 首屏 UI 绘制前，把 BSP 的 50 行缓冲换成 PSRAM 全帧。 */
    if (!board_display_lock(1000)) {
        ESP_LOGE(TAG, "display lock timeout");
        return ESP_ERR_TIMEOUT;
    }
    err = board_display_install_full_buffers();
    board_display_unlock();
    ESP_RETURN_ON_ERROR(err, TAG, "framebuffer upgrade failed");

    ESP_RETURN_ON_ERROR(bsp_display_backlight_on(), TAG, "backlight on failed");
    ESP_RETURN_ON_ERROR(board_brightness_set(70), TAG, "brightness failed");

    ESP_LOGI(TAG, "board ready");
    return ESP_OK;
}

bool board_display_lock(uint32_t timeout_ms)
{
    /* bsp_display_lock() 返回 esp_err_t，ESP_OK 为 0，不能当 bool 用。 */
    return bsp_display_lock(timeout_ms) == ESP_OK;
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
