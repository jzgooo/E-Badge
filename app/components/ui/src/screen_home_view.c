#include "screens.h"
#include "fonts/font_cn.h"

#include "badge.h"
#include "board.h"

#include "esp_timer.h"
#include "lvgl.h"

#include <stdio.h>

static lv_obj_t *s_arc_track;
static lv_obj_t *s_arc;
static lv_obj_t *s_label_brand;
static lv_obj_t *s_label_center;
static lv_obj_t *s_label_caption;
static lv_obj_t *s_label_hint;
static lv_obj_t *s_label_batt;
static lv_obj_t *s_batt_dot;
static lv_timer_t *s_batt_timer;
static bool s_home_active;
static bool s_dot_on;
static int64_t s_last_blink_us;
static badge_card_id_t s_manual_card = BADGE_CARD_INVALID;

#define COL_BG        0x0c0f14
#define COL_TRACK     0x1e242c
#define COL_RING_SOFT 0x161b22
#define COL_ACCENT    0x3dceb0
#define COL_EMPTY     0x3a4550
#define COL_STALE     0x8a8075
#define COL_TEXT      0xf2efe8
#define COL_MUTED     0x7a8494
#define COL_HINT      0x4a5563
#define COL_CHARGE    0x3dceb0
#define COL_DANGER    0xc45c5c
#define COL_WARN      0xd6a75a
#define BATT_REFRESH_MS 1000

static const char *card_brand(badge_card_id_t id)
{
    switch (id) {
    case BADGE_CARD_QUOTA: return "额度";
    case BADGE_CARD_BUILD: return "构建";
    case BADGE_CARD_FOCUS: return "专注";
    case BADGE_CARD_SCHEDULE: return "日程";
    default: return "状态";
    }
}

static void stop_batt_timer(void)
{
    if (s_batt_timer) {
        lv_timer_delete(s_batt_timer);
        s_batt_timer = NULL;
    }
}

static void style_arc(lv_obj_t *arc, uint32_t color, int width)
{
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(COL_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR);
}

static void paint_battery(void)
{
    if (!s_label_batt) return;
    board_battery_t b;
    char buf[24];
    if (board_battery_get(&b) != ESP_OK || b.percent < 0) {
        lv_label_set_text(s_label_batt, "--");
        if (s_batt_dot) lv_obj_add_flag(s_batt_dot, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    snprintf(buf, sizeof(buf), "%d%%", b.percent);
    lv_label_set_text(s_label_batt, buf);
    if (b.charging) {
        lv_obj_remove_flag(s_batt_dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_batt_dot, lv_color_hex(COL_CHARGE), 0);
        const int64_t now = esp_timer_get_time();
        if (now - s_last_blink_us >= 500000) {
            s_dot_on = !s_dot_on;
            s_last_blink_us = now;
        }
        lv_obj_set_style_bg_opa(s_batt_dot, s_dot_on ? LV_OPA_COVER : LV_OPA_20, 0);
    } else if (b.percent < 15) {
        lv_obj_remove_flag(s_batt_dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_batt_dot, lv_color_hex(COL_DANGER), 0);
        lv_obj_set_style_bg_opa(s_batt_dot, LV_OPA_COVER, 0);
    } else {
        lv_obj_add_flag(s_batt_dot, LV_OBJ_FLAG_HIDDEN);
    }
}

static bool card_needs_attention(const badge_card_t *card)
{
    if (!card || badge_dashboard_card_is_stale(card)) return false;
    if (card->severity != BADGE_SEVERITY_NORMAL) return true;
    const int remaining = badge_dashboard_card_remaining_sec(card);
    return (card->id == BADGE_CARD_SCHEDULE && remaining >= 0 && remaining <= 600) ||
           (card->id == BADGE_CARD_FOCUS && remaining >= 0 && remaining <= 120);
}

static const badge_card_t *selected_dashboard_card(void)
{
    const badge_card_t *priority = badge_dashboard_select_priority();
    if (!priority || card_needs_attention(priority)) return priority;
    badge_dashboard_prefs_t prefs;
    badge_dashboard_get_prefs(&prefs);
    const badge_card_t *manual = badge_dashboard_find_card(s_manual_card);
    return manual && prefs.enabled[s_manual_card] ? manual : priority;
}

static void paint_legacy_quota(void)
{
    badge_quota_t q;
    badge_quota_get(&q);
    lv_label_set_text(s_label_brand, "额度");
    if (badge_quota_is_empty() || q.remain_percent == BADGE_REMAIN_PERCENT_UNKNOWN) {
        lv_arc_set_value(s_arc, 0);
        style_arc(s_arc, COL_EMPTY, 16);
        lv_label_set_text(s_label_center, "—");
        lv_label_set_text(s_label_caption, "连接后同步");
        lv_obj_set_style_text_color(s_label_center, lv_color_hex(COL_MUTED), 0);
        lv_obj_set_style_text_color(s_label_caption, lv_color_hex(COL_MUTED), 0);
        lv_obj_remove_flag(s_label_hint, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const bool stale = badge_quota_is_stale();
    lv_arc_set_value(s_arc, q.remain_percent > 100 ? 100 : q.remain_percent);
    style_arc(s_arc, stale ? COL_STALE : COL_ACCENT, 18);
    lv_label_set_text(s_label_center, q.remain_label[0] ? q.remain_label : "—");
    lv_label_set_text(s_label_caption, stale ? "数据过期" : q.quota_caption);
    lv_obj_set_style_text_color(s_label_center, lv_color_hex(stale ? COL_STALE : COL_TEXT), 0);
    lv_obj_set_style_text_color(s_label_caption, lv_color_hex(stale ? COL_STALE : COL_MUTED), 0);
    lv_obj_add_flag(s_label_hint, LV_OBJ_FLAG_HIDDEN);
}

static void paint_dashboard_card(void)
{
    const badge_card_t *card = selected_dashboard_card();
    if (!card) {
        lv_label_set_text(s_label_brand, "状态");
        lv_arc_set_value(s_arc, 0);
        style_arc(s_arc, COL_EMPTY, 16);
        lv_label_set_text(s_label_center, "—");
        lv_label_set_text(s_label_caption, "等待同步");
        lv_obj_set_style_text_color(s_label_center, lv_color_hex(COL_MUTED), 0);
        lv_obj_set_style_text_color(s_label_caption, lv_color_hex(COL_MUTED), 0);
        lv_obj_remove_flag(s_label_hint, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const bool needs_sync = badge_dashboard_card_needs_sync(card);
    const bool stale = badge_dashboard_card_is_stale(card);
    uint32_t color = COL_ACCENT;
    if (stale) color = COL_STALE;
    else if (card->severity == BADGE_SEVERITY_CRITICAL) color = COL_DANGER;
    else if (card->severity == BADGE_SEVERITY_NEAR) color = COL_WARN;
    lv_label_set_text(s_label_brand, card_brand(card->id));
    lv_arc_set_value(s_arc, card->percent == BADGE_CARD_PERCENT_UNKNOWN ? 100 : card->percent);
    style_arc(s_arc, color, card->percent == BADGE_CARD_PERCENT_UNKNOWN ? 10 : 18);
    const int remaining = badge_dashboard_card_remaining_sec(card);
    char countdown[16];
    if ((card->id == BADGE_CARD_FOCUS || card->id == BADGE_CARD_SCHEDULE) && remaining >= 0) {
        snprintf(countdown, sizeof(countdown), "%02d:%02d", remaining / 60, remaining % 60);
        lv_label_set_text(s_label_center, countdown);
    } else {
        lv_label_set_text(s_label_center, card->label[0] ? card->label : "—");
    }
    lv_obj_set_style_text_color(s_label_center, lv_color_hex(stale ? COL_STALE : COL_TEXT), 0);
    if (needs_sync) {
        lv_label_set_text(s_label_caption, "等待同步");
        lv_obj_set_style_text_color(s_label_caption, lv_color_hex(COL_STALE), 0);
    } else if (stale) {
        lv_label_set_text(s_label_caption, "数据过期");
        lv_obj_set_style_text_color(s_label_caption, lv_color_hex(COL_STALE), 0);
    } else {
        lv_label_set_text(s_label_caption, card->caption);
        lv_obj_set_style_text_color(s_label_caption, lv_color_hex(COL_MUTED), 0);
    }
    lv_obj_add_flag(s_label_hint, LV_OBJ_FLAG_HIDDEN);
}

static void paint_home_card(void)
{
    if (badge_dashboard_is_configured()) paint_dashboard_card();
    else paint_legacy_quota();
}

static void on_batt_timer(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!s_home_active) return;
    paint_home_card();
    paint_battery();
}

void screen_open_settings(void)
{
    if (!s_home_active) return;
    s_home_active = false;
    stop_batt_timer();
    screen_settings_show();
}

bool screen_home_is_active(void) { return s_home_active; }

void screen_home_refresh(void)
{
    if (!s_home_active) return;
    paint_home_card();
    paint_battery();
}

void screen_home_next_card(void)
{
    if (!badge_dashboard_is_configured()) return;
    badge_dashboard_prefs_t prefs;
    badge_dashboard_get_prefs(&prefs);
    int current = -1;
    for (int i = 0; i < BADGE_DASHBOARD_MAX_CARDS; ++i) {
        if (prefs.order[i] == s_manual_card) current = i;
    }
    for (int step = 1; step <= BADGE_DASHBOARD_MAX_CARDS; ++step) {
        const badge_card_id_t id = prefs.order[(current + step) % BADGE_DASHBOARD_MAX_CARDS];
        if (prefs.enabled[id] && badge_dashboard_find_card(id)) {
            s_manual_card = id;
            screen_home_refresh();
            return;
        }
    }
}

static void on_home_tapped(lv_event_t *event)
{
    LV_UNUSED(event);
    screen_home_next_card();
}

void screen_home_show(void)
{
    stop_batt_timer();
    s_label_batt = NULL;
    s_batt_dot = NULL;
    s_dot_on = true;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, on_home_tapped, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *halo = lv_obj_create(scr);
    lv_obj_set_size(halo, 420, 420);
    lv_obj_center(halo);
    lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(halo, lv_color_hex(COL_RING_SOFT), 0);
    lv_obj_set_style_bg_opa(halo, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(halo, 1, 0);
    lv_obj_set_style_border_color(halo, lv_color_hex(0x24303a), 0);
    lv_obj_remove_flag(halo, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(halo, 0, 0);

    s_arc_track = lv_arc_create(scr);
    lv_obj_set_size(s_arc_track, 360, 360);
    lv_obj_center(s_arc_track);
    lv_arc_set_rotation(s_arc_track, 270);
    lv_arc_set_bg_angles(s_arc_track, 0, 360);
    lv_arc_set_range(s_arc_track, 0, 100);
    lv_arc_set_value(s_arc_track, 100);
    style_arc(s_arc_track, COL_TRACK, 6);
    lv_obj_set_style_arc_color(s_arc_track, lv_color_hex(0x141920), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_track, lv_color_hex(0x1a222c), LV_PART_INDICATOR);

    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, 360, 360);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_mode(s_arc, LV_ARC_MODE_NORMAL);
    style_arc(s_arc, COL_ACCENT, 18);

    s_batt_dot = lv_obj_create(scr);
    lv_obj_set_size(s_batt_dot, 10, 10);
    lv_obj_align(s_batt_dot, LV_ALIGN_TOP_MID, -34, 32);
    lv_obj_set_style_radius(s_batt_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_batt_dot, 0, 0);
    lv_obj_set_style_pad_all(s_batt_dot, 0, 0);
    lv_obj_remove_flag(s_batt_dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_batt_dot, LV_OBJ_FLAG_HIDDEN);

    s_label_batt = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_batt, lv_color_hex(COL_MUTED), 0);
    lv_obj_set_style_text_font(s_label_batt, &font_cn_16, 0);
    lv_obj_align(s_label_batt, LV_ALIGN_TOP_MID, 8, 28);
    lv_obj_remove_flag(s_label_batt, LV_OBJ_FLAG_CLICKABLE);

    s_label_brand = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_brand, lv_color_hex(0x4f5d6b), 0);
    lv_obj_set_style_text_font(s_label_brand, &font_cn_16, 0);
    lv_obj_set_style_text_letter_space(s_label_brand, 0, 0);
    lv_obj_align(s_label_brand, LV_ALIGN_CENTER, 0, -58);
    lv_obj_remove_flag(s_label_brand, LV_OBJ_FLAG_CLICKABLE);

    s_label_center = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_center, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(s_label_center, &lv_font_montserrat_28, 0);
    lv_obj_align(s_label_center, LV_ALIGN_CENTER, 0, -4);
    lv_obj_remove_flag(s_label_center, LV_OBJ_FLAG_CLICKABLE);

    s_label_caption = lv_label_create(scr);
    lv_obj_set_style_text_font(s_label_caption, &font_cn_16, 0);
    lv_obj_align(s_label_caption, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_text_align(s_label_caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_label_caption, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_label_caption, 220);
    lv_obj_remove_flag(s_label_caption, LV_OBJ_FLAG_CLICKABLE);

    s_label_hint = lv_label_create(scr);
    lv_label_set_text(s_label_hint, "等待同步");
    lv_obj_set_style_text_color(s_label_hint, lv_color_hex(COL_HINT), 0);
    lv_obj_set_style_text_font(s_label_hint, &font_cn_16, 0);
    lv_obj_align(s_label_hint, LV_ALIGN_CENTER, 0, 68);
    lv_obj_remove_flag(s_label_hint, LV_OBJ_FLAG_CLICKABLE);

    s_home_active = true;
    paint_home_card();
    paint_battery();
    s_batt_timer = lv_timer_create(on_batt_timer, BATT_REFRESH_MS, NULL);
}
