#include "board.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "lvgl.h"

static const char *TAG = "board";

static const char *NVS_NS = "board";
static const char *KEY_BRI = "bri";
static const char *KEY_TO = "to";

#define BRI_DEFAULT 70
#define BRI_MIN 10
#define BRI_LOW_BATT_CAP 35
#define LOW_BATT_PERCENT 15
#define TO_DEFAULT 10

static int s_brightness_percent = BRI_DEFAULT;
static int s_timeout_sec = TO_DEFAULT;
static bool s_asleep;
static lv_display_t *s_disp;

extern esp_err_t board_pmu_init(void);
extern esp_err_t board_pmu_start(void);
extern esp_err_t board_pmu_shutdown(void);
extern void board_pmu_set_event_cb(board_pwr_event_cb_t cb);

/* ESP32-S3 数据缓存行 64 字节，PSRAM DMA 需要按此对齐。 */
#define BOARD_FB_ALIGN 64

static int clamp_bri(int percent)
{
    if (percent < BRI_MIN) {
        return BRI_MIN;
    }
    if (percent > 100) {
        return 100;
    }
    return percent;
}

static int sanitize_timeout(int sec)
{
    if (sec == 5 || sec == 10 || sec == 30) {
        return sec;
    }
    return TO_DEFAULT;
}

static esp_err_t nvs_load_prefs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    uint8_t bri = (uint8_t)s_brightness_percent;
    uint8_t to = (uint8_t)s_timeout_sec;
    if (nvs_get_u8(h, KEY_BRI, &bri) == ESP_OK) {
        s_brightness_percent = clamp_bri(bri);
    }
    if (nvs_get_u8(h, KEY_TO, &to) == ESP_OK) {
        s_timeout_sec = sanitize_timeout(to);
    }
    nvs_close(h);
    return ESP_OK;
}

static esp_err_t nvs_save_u8(const char *key, uint8_t value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(h, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

static int effective_brightness(void)
{
    if (s_asleep) {
        return 0;
    }

    int bri = s_brightness_percent;
    board_battery_t b;
    if (board_battery_get(&b) == ESP_OK && b.present && !b.charging && b.percent >= 0 &&
        b.percent < LOW_BATT_PERCENT) {
        if (bri > BRI_LOW_BATT_CAP) {
            bri = BRI_LOW_BATT_CAP;
        }
    }
    return bri;
}

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

    (void)nvs_load_prefs();

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
    s_asleep = false;
    board_brightness_reapply();

    /* 电量可读即可；失败不阻断开机。 */
    if (board_pmu_init() != ESP_OK) {
        ESP_LOGW(TAG, "PMU init failed, battery UI will show --");
    } else if (board_pmu_start() != ESP_OK) {
        ESP_LOGW(TAG, "PMU event poll failed to start");
    }

    ESP_LOGI(TAG, "board ready bri=%d to=%ds", s_brightness_percent, s_timeout_sec);
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
    s_brightness_percent = clamp_bri(percent);
    esp_err_t err = nvs_save_u8(KEY_BRI, (uint8_t)s_brightness_percent);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "brightness nvs save failed: %s", esp_err_to_name(err));
    }
    board_brightness_reapply();
    return ESP_OK;
}

int board_brightness_get(void)
{
    return s_brightness_percent;
}

void board_brightness_reapply(void)
{
    int bri = effective_brightness();
    esp_err_t err = bsp_display_brightness_set(bri);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "brightness set %d failed: %s", bri, esp_err_to_name(err));
    }
}

esp_err_t board_display_sleep(void)
{
    if (s_asleep) {
        return ESP_OK;
    }
    s_asleep = true;
    ESP_LOGI(TAG, "display sleep");
    return bsp_display_backlight_off();
}

esp_err_t board_display_wake(void)
{
    if (!s_asleep) {
        board_brightness_reapply();
        return ESP_OK;
    }
    s_asleep = false;
    ESP_LOGI(TAG, "display wake");
    board_brightness_reapply();
    return ESP_OK;
}

bool board_display_is_asleep(void)
{
    return s_asleep;
}

int board_sleep_timeout_sec_get(void)
{
    return s_timeout_sec;
}

esp_err_t board_sleep_timeout_sec_set(int sec)
{
    s_timeout_sec = sanitize_timeout(sec);
    esp_err_t err = nvs_save_u8(KEY_TO, (uint8_t)s_timeout_sec);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "timeout nvs save failed: %s", esp_err_to_name(err));
    }
    return ESP_OK;
}

void board_pwr_set_event_cb(board_pwr_event_cb_t cb)
{
    board_pmu_set_event_cb(cb);
}

esp_err_t board_shutdown(void)
{
    ESP_LOGW(TAG, "shutdown requested");
    return board_pmu_shutdown();
}
