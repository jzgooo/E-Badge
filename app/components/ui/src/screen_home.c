#include "screens.h"

#include "badge.h"
#include "board.h"

#include "lvgl.h"

static lv_obj_t *s_arc;
static lv_obj_t *s_label_center;
static lv_obj_t *s_label_caption;
static bool s_home_active;

static void on_long_press_settings(lv_event_t *event)
{
    LV_UNUSED(event);
    s_home_active = false;
    screen_settings_show();
}

static void paint_quota(void)
{
    if (!s_arc || !s_label_center || !s_label_caption) {
        return;
    }

    badge_quota_t q;
    badge_quota_get(&q);

    if (badge_quota_is_empty() || q.remain_percent == BADGE_REMAIN_PERCENT_UNKNOWN) {
        lv_arc_set_value(s_arc, 0);
        lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x3a424c), LV_PART_INDICATOR);
        lv_label_set_text(s_label_center, "");
        lv_label_set_text(s_label_caption, "Sync via BLE");
        return;
    }

    int pct = q.remain_percent;
    if (pct > 100) {
        pct = 100;
    }
    lv_arc_set_value(s_arc, pct);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x5ec8ff), LV_PART_INDICATOR);
    lv_label_set_text(s_label_center, q.remain_label[0] ? q.remain_label : "");
    lv_label_set_text(s_label_caption, q.quota_caption[0] ? q.quota_caption : "");
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
    board_display_unlock();
}

void screen_home_show(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0b0e12), 0);

    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, 340, 340);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_mode(s_arc, LV_ARC_MODE_NORMAL);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_arc, 18, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 18, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x2a3038), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_INDICATOR);

    s_label_center = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_center, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_label_center, &lv_font_montserrat_28, 0);
    lv_obj_align(s_label_center, LV_ALIGN_CENTER, 0, -8);

    s_label_caption = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_caption, lv_color_hex(0x9aa4b2), 0);
    lv_obj_set_style_text_font(s_label_caption, &lv_font_montserrat_16, 0);
    lv_obj_align(s_label_caption, LV_ALIGN_CENTER, 0, 36);
    lv_obj_set_style_text_align(s_label_caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_label_caption, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_label_caption, 280);

    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, on_long_press_settings, LV_EVENT_LONG_PRESSED, NULL);

    s_home_active = true;
    paint_quota();
}
