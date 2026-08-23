#include "ble.h"
#include "badge.h"

#include <string.h>

#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble";
static const char *DEVICE_NAME = "Codex";

void ble_store_config_init(void);

static uint8_t s_own_addr_type;

/* Locked GATT UUIDs (also documented in docs/PRD.md §5). */
static const ble_uuid16_t s_quota_svc_uuid = BLE_UUID16_INIT(0xFF00);
static const ble_uuid16_t s_quota_chr_uuid = BLE_UUID16_INIT(0xFF01);
static const ble_uuid16_t s_dashboard_chr_uuid = BLE_UUID16_INIT(0xFF02);

#define QUOTA_JSON_MAX 192
#define DASHBOARD_JSON_MAX 1024

static int quota_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        char buf[QUOTA_JSON_MAX];
        size_t n = 0;
        esp_err_t err = badge_quota_to_json(buf, sizeof(buf), &n);
        if (err != ESP_OK) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        int rc = os_mbuf_append(ctxt->om, buf, n);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len == 0 || om_len >= QUOTA_JSON_MAX) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        char buf[QUOTA_JSON_MAX];
        uint16_t copied = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &copied);
        if (rc != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        buf[copied] = '\0';

        esp_err_t err = badge_quota_apply_json(buf, copied);
        if (err == ESP_ERR_INVALID_ARG) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        if (err != ESP_OK) {
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static int dashboard_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        char buf[DASHBOARD_JSON_MAX];
        size_t n = 0;
        esp_err_t err = badge_dashboard_to_json(buf, sizeof(buf), &n);
        if (err == ESP_ERR_INVALID_SIZE) {
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (err != ESP_OK) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        return os_mbuf_append(ctxt->om, buf, n) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        const uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len == 0 || om_len >= DASHBOARD_JSON_MAX) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        char buf[DASHBOARD_JSON_MAX];
        uint16_t copied = 0;
        if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &copied) != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        buf[copied] = '\0';
        const esp_err_t err = badge_dashboard_apply_json(buf, copied);
        if (err == ESP_ERR_INVALID_SIZE) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        if (err == ESP_ERR_INVALID_ARG) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        return err == ESP_OK ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_quota_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &s_quota_chr_uuid.u,
                .access_cb = quota_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = &s_dashboard_chr_uuid.u,
                .access_cb = dashboard_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {0},
        },
    },
    {0},
};

static int gap_event_handler(struct ble_gap_event *event, void *arg);

static void start_advertising(void)
{
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_gap_adv_params adv_params = {0};
    const char *name = ble_svc_gap_device_name();

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "advertising as %s", name);
}

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            start_advertising();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;
    default:
        break;
    }
    return 0;
}

static void on_stack_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "no BLE address: %d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }

    start_advertising();
}

static void on_stack_reset(int reason)
{
    ESP_LOGW(TAG, "nimble reset, reason=%d", reason);
}

static void nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_start(void)
{
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "device name set failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return ESP_FAIL;
    }

    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();

    nimble_port_freertos_init(nimble_host_task);
    ESP_LOGI(TAG, "ble started");
    return ESP_OK;
}
