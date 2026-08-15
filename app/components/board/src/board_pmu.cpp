#include "board.h"

#include <cstdlib>
#include <cstring>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

#include "bsp/esp-bsp.h"

#include "XPowersLib.h"

static const char *TAG = "board_pmu";

#define PMU_I2C_TIMEOUT_MS 1000
#define PMU_I2C_FREQ_HZ    400000

static XPowersPMU s_pmu;
static i2c_master_dev_handle_t s_pmu_dev;
static bool s_pmu_ok;

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

    /* 只读电量/电压，不改各路电源开关。 */
    s_pmu.enableBattVoltageMeasure();
    s_pmu_ok = true;
    ESP_LOGI(TAG, "AXP2101 ready, battery=%d%% %dmV charging=%d",
             s_pmu.getBatteryPercent(), (int)s_pmu.getBattVoltage(),
             (int)s_pmu.isCharging());
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
        int pct = s_pmu.getBatteryPercent();
        out->percent = (pct >= 0 && pct <= 100) ? pct : -1;
        out->millivolts = (int)s_pmu.getBattVoltage();
    }
    return ESP_OK;
}
