#include "board.h"

#include <cstdlib>
#include <cstring>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp/esp-bsp.h"

#include "XPowersLib.h"

static const char *TAG = "board_pmu";

#define PMU_I2C_TIMEOUT_MS 1000
#define PMU_I2C_FREQ_HZ    400000
#define PMU_POLL_MS        80

/* 满电判定：低于此电压时不显示 100%（AXP 电量计常在 ~4.14V 就报满）。 */
#define BATT_FULL_MV 4180

static XPowersPMU s_pmu;
static i2c_master_dev_handle_t s_pmu_dev;
static bool s_pmu_ok;
static board_pwr_event_cb_t s_pwr_cb;

/**
 * 单节锂电开路电压粗估 SOC。充电时端电压偏高，调用方需自行折中。
 * 表点为典型 3.7V 聚合物电芯近似值。
 */
static int soc_from_voltage_mv(int mv)
{
    static const int points[][2] = {
        {4200, 100}, {4150, 95},  {4110, 90},  {4070, 85},  {4030, 80},
        {3990, 75},  {3950, 70},  {3910, 65},  {3870, 60},  {3830, 55},
        {3790, 50},  {3750, 45},  {3710, 40},  {3670, 35},  {3630, 30},
        {3590, 25},  {3550, 20},  {3500, 15},  {3440, 10},  {3360, 5},
        {3300, 0},
    };
    const int n = (int)(sizeof(points) / sizeof(points[0]));

    if (mv >= points[0][0]) {
        return 100;
    }
    if (mv <= points[n - 1][0]) {
        return 0;
    }
    for (int i = 0; i < n - 1; i++) {
        const int hi_mv = points[i][0];
        const int lo_mv = points[i + 1][0];
        if (mv <= hi_mv && mv >= lo_mv) {
            const int hi_pct = points[i][1];
            const int lo_pct = points[i + 1][1];
            const int span = hi_mv - lo_mv;
            if (span <= 0) {
                return hi_pct;
            }
            return lo_pct + (hi_pct - lo_pct) * (mv - lo_mv) / span;
        }
    }
    return 0;
}

static int resolve_battery_percent(int gauge_pct, int mv, bool charging)
{
    const int from_v = soc_from_voltage_mv(mv);

    if (gauge_pct < 0 || gauge_pct > 100) {
        return from_v;
    }

    /* 电量计报 100% 但电压未到满电：按电压显示，最高 99。 */
    if (gauge_pct >= 100 && mv > 0 && mv < BATT_FULL_MV) {
        return from_v < 100 ? from_v : 99;
    }

    if (charging) {
        /* 充电时端电压虚高，电压估会偏乐观；以电量计为主，但不超过电压估+5。 */
        int capped = from_v + 5;
        if (capped > 100) {
            capped = 100;
        }
        return gauge_pct < capped ? gauge_pct : capped;
    }

    /* 静置：电压曲线通常比未标定电量计更贴近观感。 */
    return from_v;
}

static int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    (void)devAddr;
    if (!s_pmu_dev) {
        return -1;
    }
    esp_err_t ret = i2c_master_transmit_receive(s_pmu_dev, &regAddr, 1, data, len, PMU_I2C_TIMEOUT_MS);
    return (ret == ESP_OK) ? 0 : -1;
}

static int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    (void)devAddr;
    if (!s_pmu_dev || !data) {
        return -1;
    }
    uint8_t *buffer = (uint8_t *)malloc(len + 1);
    if (!buffer) {
        return -1;
    }
    buffer[0] = regAddr;
    memcpy(&buffer[1], data, len);
    esp_err_t ret = i2c_master_transmit(s_pmu_dev, buffer, len + 1, PMU_I2C_TIMEOUT_MS);
    free(buffer);
    return (ret == ESP_OK) ? 0 : -1;
}

static void pmu_poll_task(void *arg)
{
    (void)arg;
    while (true) {
        if (s_pmu_ok) {
            s_pmu.getIrqStatus();
            const bool short_press = s_pmu.isPekeyShortPressIrq();
            const bool long_press = s_pmu.isPekeyLongPressIrq();
            s_pmu.clearIrqStatus();

            if (long_press) {
                ESP_LOGI(TAG, "PWR long press");
                if (s_pwr_cb) {
                    s_pwr_cb(BOARD_PWR_EVENT_LONG);
                }
            } else if (short_press) {
                ESP_LOGI(TAG, "PWR short press");
                if (s_pwr_cb) {
                    s_pwr_cb(BOARD_PWR_EVENT_SHORT);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(PMU_POLL_MS));
    }
}

extern "C" esp_err_t board_pmu_init(void)
{
    s_pmu_ok = false;
    s_pmu_dev = nullptr;

    /* 触摸已用同一总线；display_start 之后 I2C 已就绪。 */
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) {
        ESP_LOGW(TAG, "I2C bus unavailable, battery disabled");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = AXP2101_SLAVE_ADDRESS;
    dev_config.scl_speed_hz = PMU_I2C_FREQ_HZ;

    esp_err_t err = i2c_master_bus_add_device(bus, &dev_config, &s_pmu_dev);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "add AXP2101 failed: %s", esp_err_to_name(err));
        return err;
    }

    if (!s_pmu.begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte)) {
        ESP_LOGW(TAG, "AXP2101 begin failed");
        return ESP_FAIL;
    }

    s_pmu.enableBattDetection();
    s_pmu.enableBattVoltageMeasure();
    s_pmu.enableVbusVoltageMeasure();
    s_pmu.enableSystemVoltageMeasure();
    s_pmu.disableTSPinMeasure();
    /* 开启电量计；不写 BROM 电池模型（无标定数据时只能靠电压校正）。 */
    s_pmu.fuelGaugeControl(false, true);
    s_pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    /* 1.75C 上 AXP IRQ 不一定有可用 GPIO，改轮询状态寄存器。 */
    s_pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    s_pmu.clearIrqStatus();
    s_pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ);

    /* 短按只出 IRQ；长按关机时间留给软件处理（略拉长硬件关断窗口）。 */
    s_pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);

    s_pmu_ok = true;
    ESP_LOGI(TAG, "AXP2101 ready, battery=%d%% %dmV charging=%d",
             s_pmu.getBatteryPercent(), (int)s_pmu.getBattVoltage(),
             (int)s_pmu.isCharging());
    return ESP_OK;
}

extern "C" esp_err_t board_pmu_start(void)
{
    if (!s_pmu_ok) {
        return ESP_ERR_INVALID_STATE;
    }
    BaseType_t ok = xTaskCreate(pmu_poll_task, "pmu_poll", 3072, nullptr, 5, nullptr);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

extern "C" void board_pmu_set_event_cb(board_pwr_event_cb_t cb)
{
    s_pwr_cb = cb;
}

extern "C" esp_err_t board_pmu_shutdown(void)
{
    if (!s_pmu_ok) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(TAG, "AXP2101 shutdown");
    s_pmu.shutdown();
    return ESP_OK;
}

extern "C" esp_err_t board_battery_get(board_battery_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    out->percent = -1;
    out->millivolts = 0;
    out->charging = false;
    out->present = false;

    if (!s_pmu_ok) {
        return ESP_ERR_INVALID_STATE;
    }

    out->present = s_pmu.isBatteryConnect();
    out->charging = s_pmu.isCharging();
    if (out->present) {
        const int gauge = s_pmu.getBatteryPercent();
        out->millivolts = (int)s_pmu.getBattVoltage();
        out->percent = resolve_battery_percent(gauge, out->millivolts, out->charging);
    }
    return ESP_OK;
}
