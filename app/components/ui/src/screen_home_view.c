#include "screens.h"
#include "fonts/font_cn.h"

#include "badge.h"
#include "board.h"

#include "lvgl.h"

#include <stdio.h>

static lv_obj_t *s_arc_track;   /* 底环：固定铺满，衬托进度弧 */
static lv_obj_t *s_arc;         /* 进度弧：remain_percent */
static lv_obj_t *s_label_brand;
static lv_obj_t *s_label_center;
static lv_obj_t *s_label_caption;
static lv_obj_t *s_label_hint;  /* 仅空数据时显示「等待同步」 */
static lv_obj_t *s_label_batt;  /* 顶部电量小字 */
static lv_timer_t *s_batt_timer;
static bool s_home_active;

/* 圆屏主色：暖墨底 + 薄荷绿进度；过期用暖灰。 */
#define COL_BG        0x0c0f14
#define COL_TRACK     0x1e242c
#define COL_RING_SOFT 0x161b22
#define COL_ACCENT    0x3dceb0
#define COL_EMPTY     0x3a4550
#define COL_STALE     0x8a8075
#define COL_TEXT      0xf2efe8
#define COL_MUTED     0x7a8494
#define COL_HINT      0x4a5563

#define BATT_REFRESH_MS 5000

static void stop_batt_timer(void)
{
    if (s_batt_timer) {
        lv_timer_delete(s_batt_timer);
        s_batt_timer = NULL;
    }
}

static void paint_battery(void)
{
    if (!s_label_batt) {
        return;
    }
    board_battery_t b;
    char buf[24];
    if (board_battery_get(&b) != ESP_OK || b.percent < 0) {
        snprintf(buf, sizeof(buf), "--");
    } else if (b.charging) {
        snprintf(buf, sizeof(buf), "%d%% 充电", b.percent);
    } else {
        snprintf(buf, sizeof(buf), "%d%%", b.percent);
    }
    lv_label_set_text(s_label_batt, buf);
}

static void on_batt_timer(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!s_home_active) {
        return;
    }
    paint_battery();
}

static void on_open_settings(lv_event_t *event)
{
    LV_UNUSED(event);
    s_home_active = false;
    stop_batt_timer();
    screen_settings_show();
}

static void style_quota_arc(lv_obj_t *arc, uint32_t color, int width)
{
    /* 去掉 knob，避免可拖；进度色走 INDICATOR。 */
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(COL_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR);
}

static void paint_quota(void)
{
    if (!s_arc || !s_label_center || !s_label_caption) {
        return;
    }

    badge_quota_t q;
    badge_quota_get(&q);

    if (badge_quota_is_empty() || q.remain_percent == BADGE_REMAIN_PERCENT_UNKNOWN) {
        /* 从未同步：空环 + 占位，提示走 BLE。 */
        lv_arc_set_value(s_arc, 0);
        style_quota_arc(s_arc, COL_EMPTY, 16);
        lv_label_set_text(s_label_center, "—");
        lv_obj_set_style_text_color(s_label_center, lv_color_hex(COL_MUTED), 0);
        lv_label_set_text(s_label_caption, "连接后同步");
        lv_obj_set_style_text_color(s_label_caption, lv_color_hex(COL_MUTED), 0);
        if (s_label_hint) {
            lv_obj_remove_flag(s_label_hint, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    int pct = q.remain_percent;
    if (pct > 100) {
        pct = 100;
    }

    const bool stale = badge_quota_is_stale();
    /* 过期仍画上次比例，但环与文案改暖灰（见 PRD）。 */
    const uint32_t ring = stale ? COL_STALE : COL_ACCENT;
    lv_arc_set_value(s_arc, pct);
    style_quota_arc(s_arc, ring, 18);

    lv_label_set_text(s_label_center, q.remain_label[0] ? q.remain_label : "—");
    lv_obj_set_style_text_color(s_label_center, lv_color_hex(COL_TEXT), 0);

    if (stale) {
        lv_label_set_text(s_label_caption, "数据过期");
        lv_obj_set_style_text_color(s_label_caption, lv_color_hex(COL_STALE), 0);
    } else {
        lv_label_set_text(s_label_caption, q.quota_caption[0] ? q.quota_caption : "");
        lv_obj_set_style_text_color(s_label_caption, lv_color_hex(COL_MUTED), 0);
    }

    if (s_label_hint) {
        lv_obj_add_flag(s_label_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

void screen_home_refresh(void)
{
    if (!s_home_active) {
        return;
    }
    if (!board_display_lock(200)) {
        return;
    }
    paint_quota();
    paint_battery();
    board_display_unlock();
}

void screen_home_show(void)
{
    stop_batt_timer();
    s_label_batt = NULL;

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 外圈柔光：装饰用，禁止点击以免挡设置按钮。 */
    lv_obj_t *halo = lv_obj_create(scr);
    lv_obj_set_size(halo, 420, 420);
    lv_obj_center(halo);
    lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(halo, lv_color_hex(COL_RING_SOFT), 0);
    lv_obj_set_style_bg_opa(halo, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(halo, 1, 0);
    lv_obj_set_style_border_color(halo, lv_color_hex(0x24303a), 0);
    lv_obj_set_style_border_opa(halo, LV_OPA_60, 0);
    lv_obj_remove_flag(halo, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(halo, 0, 0);

    /* 双环：细底环恒满 + 粗进度环叠在上面。 */
    s_arc_track = lv_arc_create(scr);
    lv_obj_set_size(s_arc_track, 360, 360);
    lv_obj_center(s_arc_track);
    lv_arc_set_rotation(s_arc_track, 270);
    lv_arc_set_bg_angles(s_arc_track, 0, 360);
    lv_arc_set_range(s_arc_track, 0, 100);
    lv_arc_set_value(s_arc_track, 100);
    style_quota_arc(s_arc_track, COL_TRACK, 6);
    lv_obj_set_style_arc_color(s_arc_track, lv_color_hex(0x141920), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_track, lv_color_hex(0x1a222c), LV_PART_INDICATOR);

    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, 360, 360);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_mode(s_arc, LV_ARC_MODE_NORMAL);
    style_quota_arc(s_arc, COL_ACCENT, 18);

    /* 顶部电量，不抢额度环中心。 */
    s_label_batt = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_batt, lv_color_hex(COL_MUTED), 0);
    lv_obj_set_style_text_font(s_label_batt, &font_cn_16, 0);
    lv_obj_align(s_label_batt, LV_ALIGN_TOP_MID, 0, 28);

    s_label_brand = lv_label_create(scr);
    lv_label_set_text(s_label_brand, "CODEX");
    lv_obj_set_style_text_color(s_label_brand, lv_color_hex(0x4f5d6b), 0);
    lv_obj_set_style_text_font(s_label_brand, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_letter_space(s_label_brand, 4, 0);
    lv_obj_align(s_label_brand, LV_ALIGN_CENTER, 0, -58);

    s_label_center = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_center, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(s_label_center, &lv_font_montserrat_28, 0);
    lv_obj_align(s_label_center, LV_ALIGN_CENTER, 0, -4);

    s_label_caption = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_caption, lv_color_hex(COL_MUTED), 0);
    lv_obj_set_style_text_font(s_label_caption, &font_cn_16, 0);
    lv_obj_align(s_label_caption, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_text_align(s_label_caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_label_caption, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_label_caption, 220);

    s_label_hint = lv_label_create(scr);
    lv_label_set_text(s_label_hint, "等待同步");
    lv_obj_set_style_text_color(s_label_hint, lv_color_hex(COL_HINT), 0);
    lv_obj_set_style_text_font(s_label_hint, &font_cn_16, 0);
    lv_obj_align(s_label_hint, LV_ALIGN_CENTER, 0, 68);

    /* 独立按钮进设置；勿用整屏 LONG_PRESSED（父对象事件在 clean 后仍可能残留）。 */
    lv_obj_t *settings = lv_button_create(scr);
    lv_obj_set_size(settings, 88, 36);
    lv_obj_align(settings, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_radius(settings, 18, 0);
    lv_obj_set_style_bg_color(settings, lv_color_hex(0x1a222c), 0);
    lv_obj_set_style_bg_opa(settings, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(settings, 0, 0);
    lv_obj_set_style_border_width(settings, 1, 0);
    lv_obj_set_style_border_color(settings, lv_color_hex(0x2c3844), 0);
    lv_obj_add_event_cb(settings, on_open_settings, LV_EVENT_CLICKED, NULL);
    lv_obj_t *settings_lab = lv_label_create(settings);
    lv_label_set_text(settings_lab, "设置");
    lv_obj_set_style_text_font(settings_lab, &font_cn_16, 0);
    lv_obj_set_style_text_color(settings_lab, lv_color_hex(COL_MUTED), 0);
    lv_obj_center(settings_lab);

    s_home_active = true;
    paint_quota();
    paint_battery();
    s_batt_timer = lv_timer_create(on_batt_timer, BATT_REFRESH_MS, NULL);
}
