#include "screens.h"
#include "fonts/font_cn.h"

#include "app_version.h"
#include "badge.h"
#include "board.h"

#include "lvgl.h"

#include <stdint.h>
#include <stdio.h>

static lv_obj_t *s_timeout_btns[3];
static lv_obj_t *s_lab_batt;
static lv_timer_t *s_batt_timer;

/* 与主页同色板，设置页面板略提亮。 */
#define COL_BG       0x0c0f14
#define COL_PANEL    0x151b22
#define COL_BORDER   0x273240
#define COL_ACCENT   0x3dceb0
#define COL_TEXT     0xf2efe8
#define COL_MUTED    0x7a8494
#define COL_DANGER   0xc45c5c

#define BATT_REFRESH_MS 5000

static void stop_batt_timer(void)
{
    if (s_batt_timer) {
        lv_timer_delete(s_batt_timer);
        s_batt_timer = NULL;
    }
}

static void paint_battery_label(void)
{
    if (!s_lab_batt) {
        return;
    }
    board_battery_t b;
    char batt[48];
    if (board_battery_get(&b) != ESP_OK || b.percent < 0) {
        snprintf(batt, sizeof(batt), "电量: --");
    } else if (b.millivolts > 0 && b.charging) {
        snprintf(batt, sizeof(batt), "电量: %d%% 充电 %.2fV", b.percent,
                 b.millivolts / 1000.0);
    } else if (b.millivolts > 0) {
        snprintf(batt, sizeof(batt), "电量: %d%% %.2fV", b.percent,
                 b.millivolts / 1000.0);
    } else if (b.charging) {
        snprintf(batt, sizeof(batt), "电量: %d%% 充电", b.percent);
    } else {
        snprintf(batt, sizeof(batt), "电量: %d%%", b.percent);
    }
    lv_label_set_text(s_lab_batt, batt);
}

static void on_batt_timer(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    paint_battery_label();
}

/* 圆角内容块：亮度 / 息屏分区用。 */
static void style_panel(lv_obj_t *obj)
{
    lv_obj_set_style_radius(obj, 22, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/* 息屏 5/10/30 秒分段芯片：选中用强调色。 */
static void style_chip(lv_obj_t *btn, bool selected)
{
    lv_obj_set_style_radius(btn, 16, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    if (selected) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(COL_ACCENT), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(COL_ACCENT), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), lv_color_hex(0x0c0f14), 0);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a222c), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(COL_BORDER), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), lv_color_hex(COL_TEXT), 0);
    }
}

static void refresh_timeout_chips(void)
{
    static const int timeouts[] = {5, 10, 30};
    const int cur = board_sleep_timeout_sec_get();
    for (int i = 0; i < 3; i++) {
        if (s_timeout_btns[i]) {
            style_chip(s_timeout_btns[i], timeouts[i] == cur);
        }
    }
}

static void on_back_home(lv_event_t *event)
{
    LV_UNUSED(event);
    stop_batt_timer();
    s_lab_batt = NULL;
    screen_home_show();
}

static void on_brightness(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_target(event);
    int v = (int)lv_slider_get_value(slider);
    board_brightness_set(v);
}

static void on_timeout_clicked(lv_event_t *event)
{
    int sec = (int)(intptr_t)lv_event_get_user_data(event);
    board_sleep_timeout_sec_set(sec);
    refresh_timeout_chips();
    lv_display_trigger_activity(NULL);
}

static void on_clear_quota(lv_event_t *event)
{
    LV_UNUSED(event);
    stop_batt_timer();
    s_lab_batt = NULL;
    badge_dashboard_clear();
    badge_quota_clear();
    screen_home_show();
}

static void on_open_cards(lv_event_t *event)
{
    LV_UNUSED(event);
    stop_batt_timer();
    s_lab_batt = NULL;
    screen_dashboard_settings_show();
}

void screen_settings_show(void)
{
    stop_batt_timer();
    s_lab_batt = NULL;

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "设置");
    lv_obj_set_style_text_color(title, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(title, &font_cn_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 42);

    /* 亮度面板 */
    lv_obj_t *panel_bri = lv_obj_create(scr);
    lv_obj_set_size(panel_bri, 300, 88);
    lv_obj_align(panel_bri, LV_ALIGN_TOP_MID, 0, 88);
    style_panel(panel_bri);

    lv_obj_t *lab_bri = lv_label_create(panel_bri);
    lv_label_set_text(lab_bri, "亮度");
    lv_obj_set_style_text_color(lab_bri, lv_color_hex(COL_MUTED), 0);
    lv_obj_set_style_text_font(lab_bri, &font_cn_16, 0);
    lv_obj_align(lab_bri, LV_ALIGN_TOP_LEFT, 18, 12);

    lv_obj_t *slider = lv_slider_create(panel_bri);
    lv_obj_set_size(slider, 264, 10);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, board_brightness_get(), LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x2a3340), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(COL_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(COL_TEXT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);

    /* 息屏时间：写入 NVS，由 ui 电源定时器真正息屏 */
    lv_obj_t *panel_to = lv_obj_create(scr);
    lv_obj_set_size(panel_to, 300, 100);
    lv_obj_align(panel_to, LV_ALIGN_TOP_MID, 0, 192);
    style_panel(panel_to);

    lv_obj_t *lab_to = lv_label_create(panel_to);
    lv_label_set_text(lab_to, "息屏时间");
    lv_obj_set_style_text_color(lab_to, lv_color_hex(COL_MUTED), 0);
    lv_obj_set_style_text_font(lab_to, &font_cn_16, 0);
    lv_obj_align(lab_to, LV_ALIGN_TOP_LEFT, 18, 12);

    static const int timeouts[] = {5, 10, 30};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_button_create(panel_to);
        s_timeout_btns[i] = btn;
        lv_obj_set_size(btn, 82, 36);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, (i - 1) * 92, -14);
        lv_obj_add_event_cb(btn, on_timeout_clicked, LV_EVENT_CLICKED,
                            (void *)(intptr_t)timeouts[i]);
        lv_obj_t *lab = lv_label_create(btn);
        char buf[12];
        snprintf(buf, sizeof(buf), "%d秒", timeouts[i]);
        lv_label_set_text(lab, buf);
        lv_obj_set_style_text_font(lab, &font_cn_16, 0);
        lv_obj_center(lab);
    }
    refresh_timeout_chips();

    /* 电量+电压 / 版本：只读信息区 */
    s_lab_batt = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lab_batt, lv_color_hex(COL_MUTED), 0);
    lv_obj_set_style_text_font(s_lab_batt, &font_cn_16, 0);
    lv_obj_align(s_lab_batt, LV_ALIGN_TOP_MID, 0, 300);
    paint_battery_label();

    char ver[64];
    snprintf(ver, sizeof(ver), "v%s · %s", APP_VERSION, APP_GIT_HASH);
    lv_obj_t *lab_ver = lv_label_create(scr);
    lv_label_set_text(lab_ver, ver);
    lv_obj_set_style_text_color(lab_ver, lv_color_hex(0x4a5563), 0);
    lv_obj_set_style_text_font(lab_ver, &lv_font_montserrat_16, 0);
    lv_obj_align(lab_ver, LV_ALIGN_TOP_MID, 0, 322);

    lv_obj_t *cards_btn = lv_button_create(scr);
    lv_obj_set_size(cards_btn, 130, 44);
    lv_obj_align(cards_btn, LV_ALIGN_BOTTOM_MID, -76, -92);
    lv_obj_set_style_radius(cards_btn, 22, 0);
    lv_obj_set_style_bg_color(cards_btn, lv_color_hex(0x1a222c), 0);
    lv_obj_set_style_border_width(cards_btn, 1, 0);
    lv_obj_set_style_border_color(cards_btn, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_shadow_width(cards_btn, 0, 0);
    lv_obj_add_event_cb(cards_btn, on_open_cards, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cards_lab = lv_label_create(cards_btn);
    lv_label_set_text(cards_lab, "卡片");
    lv_obj_set_style_text_font(cards_lab, &font_cn_16, 0);
    lv_obj_set_style_text_color(cards_lab, lv_color_hex(COL_TEXT), 0);
    lv_obj_center(cards_lab);

    lv_obj_t *clear_btn = lv_button_create(scr);
    lv_obj_set_size(clear_btn, 130, 44);
    lv_obj_align(clear_btn, LV_ALIGN_BOTTOM_MID, 76, -92);
    lv_obj_set_style_radius(clear_btn, 22, 0);
    lv_obj_set_style_bg_color(clear_btn, lv_color_hex(0x2a1c1c), 0);
    lv_obj_set_style_border_width(clear_btn, 1, 0);
    lv_obj_set_style_border_color(clear_btn, lv_color_hex(COL_DANGER), 0);
    lv_obj_set_style_shadow_width(clear_btn, 0, 0);
    lv_obj_add_event_cb(clear_btn, on_clear_quota, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clear_lab = lv_label_create(clear_btn);
    lv_label_set_text(clear_lab, "清除");
    lv_obj_set_style_text_font(clear_lab, &font_cn_16, 0);
    lv_obj_set_style_text_color(clear_lab, lv_color_hex(0xe8a0a0), 0);
    lv_obj_center(clear_lab);

    lv_obj_t *back = lv_button_create(scr);
    lv_obj_set_size(back, 140, 44);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -36);
    lv_obj_set_style_radius(back, 22, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_add_event_cb(back, on_back_home, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lab = lv_label_create(back);
    lv_label_set_text(back_lab, "返回");
    lv_obj_set_style_text_font(back_lab, &font_cn_16, 0);
    lv_obj_set_style_text_color(back_lab, lv_color_hex(0x0c0f14), 0);
    lv_obj_center(back_lab);

    s_batt_timer = lv_timer_create(on_batt_timer, BATT_REFRESH_MS, NULL);
}
