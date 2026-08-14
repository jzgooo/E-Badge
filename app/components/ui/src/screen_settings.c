#include "screens.h"

#include "app_version.h"
#include "badge.h"
#include "board.h"

#include "lvgl.h"

#include <stdint.h>
#include <stdio.h>

static int s_timeout_sec = 10; /* RAM only for M1; sleep is M2 */

static void on_back_home(lv_event_t *event)
{
    LV_UNUSED(event);
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
    lv_obj_t *btn = lv_event_get_target(event);
    int sec = (int)(intptr_t)lv_event_get_user_data(event);
    s_timeout_sec = sec;
    LV_UNUSED(btn);
}

static void on_clear_quota(lv_event_t *event)
{
    LV_UNUSED(event);
    badge_quota_clear();
    screen_home_show();
}

static lv_obj_t *make_row_label(lv_obj_t *parent, const char *text, int y)
{
    lv_obj_t *lab = lv_label_create(parent);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_color(lab, lv_color_hex(0x9aa4b2), 0);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);
    lv_obj_align(lab, LV_ALIGN_TOP_MID, 0, y);
    return lab;
}

void screen_settings_show(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0b0e12), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);

    make_row_label(scr, "Brightness", 80);
    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 260);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, board_brightness_get(), LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 108);
    lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);

    make_row_label(scr, "Screen timeout (M2)", 150);

    static const int timeouts[] = {5, 10, 30};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_button_create(scr);
        lv_obj_set_size(btn, 72, 36);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, (i - 1) * 88, 182);
        lv_obj_add_event_cb(btn, on_timeout_clicked, LV_EVENT_CLICKED,
                            (void *)(intptr_t)timeouts[i]);
        lv_obj_t *lab = lv_label_create(btn);
        char buf[8];
        snprintf(buf, sizeof(buf), "%ds", timeouts[i]);
        lv_label_set_text(lab, buf);
        lv_obj_center(lab);
        if (timeouts[i] == s_timeout_sec) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a7ca5), 0);
        }
    }

    char batt[32];
    snprintf(batt, sizeof(batt), "Battery: -- (M2)");
    make_row_label(scr, batt, 236);

    char ver[64];
    snprintf(ver, sizeof(ver), "v%s (%s)", APP_VERSION, APP_GIT_HASH);
    make_row_label(scr, ver, 268);

    lv_obj_t *clear_btn = lv_button_create(scr);
    lv_obj_set_size(clear_btn, 200, 44);
    lv_obj_align(clear_btn, LV_ALIGN_BOTTOM_MID, 0, -100);
    lv_obj_add_event_cb(clear_btn, on_clear_quota, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clear_lab = lv_label_create(clear_btn);
    lv_label_set_text(clear_lab, "Clear quota");
    lv_obj_center(clear_lab);

    lv_obj_t *back = lv_button_create(scr);
    lv_obj_set_size(back, 120, 44);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_event_cb(back, on_back_home, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lab = lv_label_create(back);
    lv_label_set_text(back_lab, "Back");
    lv_obj_center(back_lab);
}
