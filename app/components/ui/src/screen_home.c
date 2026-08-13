#include "screens.h"

#include "badge.h"
#include "lvgl.h"

static void on_open_settings(lv_event_t *event)
{
    LV_UNUSED(event);
    screen_settings_show();
}

void screen_home_show(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, badge_content_get_title());
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -36);

    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle, badge_content_get_subtitle());
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x9aa4b2), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 4);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -48);
    lv_obj_add_event_cb(btn, on_open_settings, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Settings");
    lv_obj_center(btn_label);
}
