#include "screens.h"
#include "fonts/font_cn.h"

#include "badge.h"

#include "lvgl.h"

#include <stdint.h>

#define COL_BG     0x0c0f14
#define COL_PANEL  0x151b22
#define COL_BORDER 0x273240
#define COL_ACCENT 0x3dceb0
#define COL_TEXT   0xf2efe8
#define COL_MUTED  0x7a8494

static const char *const s_names[] = {"额度", "构建", "专注", "日程"};

static void style_button(lv_obj_t *button, uint32_t bg)
{
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(bg), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int x, int y, int w,
                             lv_event_cb_t cb, void *data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, w, 32);
    lv_obj_align(button, LV_ALIGN_TOP_LEFT, x, y);
    style_button(button, 0x1a222c);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, data);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &font_cn_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(COL_TEXT), 0);
    lv_obj_center(label);
    return button;
}

static void on_toggle(lv_event_t *event)
{
    const badge_card_id_t id = (badge_card_id_t)(uintptr_t)lv_event_get_user_data(event);
    badge_dashboard_prefs_t prefs;
    badge_dashboard_get_prefs(&prefs);
    (void)badge_dashboard_set_enabled(id, !prefs.enabled[id]);
    screen_dashboard_settings_show();
}

static void on_move(lv_event_t *event)
{
    const intptr_t packed = (intptr_t)lv_event_get_user_data(event);
    const badge_card_id_t id = (badge_card_id_t)(packed >> 1);
    (void)badge_dashboard_move_card(id, (packed & 1) ? 1 : -1);
    screen_dashboard_settings_show();
}

static void on_default(lv_event_t *event)
{
    (void)badge_dashboard_set_default_override((badge_card_id_t)(uintptr_t)lv_event_get_user_data(event));
    screen_dashboard_settings_show();
}

static void on_sound(lv_event_t *event)
{
    LV_UNUSED(event);
    badge_dashboard_prefs_t prefs;
    badge_dashboard_get_prefs(&prefs);
    (void)badge_dashboard_set_sound_enabled(!prefs.sound_enabled);
    screen_dashboard_settings_show();
}

static void on_back(lv_event_t *event)
{
    LV_UNUSED(event);
    screen_settings_show();
}

void screen_dashboard_settings_show(void)
{
    badge_dashboard_prefs_t prefs;
    badge_dashboard_get_prefs(&prefs);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "卡片");
    lv_obj_set_style_text_font(title, &font_cn_22, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COL_TEXT), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    for (int i = 0; i < BADGE_DASHBOARD_MAX_CARDS; ++i) {
        const badge_card_id_t id = (badge_card_id_t)i;
        const int y = 78 + i * 48;
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, 386, 42);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
        lv_obj_set_style_radius(row, 16, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(COL_PANEL), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(COL_BORDER), 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, s_names[i]);
        lv_obj_set_style_text_font(name, &font_cn_16, 0);
        lv_obj_set_style_text_color(name, lv_color_hex(COL_TEXT), 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 14, 0);
        make_button(row, prefs.enabled[i] ? "开" : "关", 116, 5, 58, on_toggle, (void *)(uintptr_t)id);
        make_button(row, "上", 182, 5, 48, on_move, (void *)(intptr_t)(id << 1));
        make_button(row, "下", 236, 5, 48, on_move, (void *)(intptr_t)((id << 1) | 1));
        lv_obj_t *def = make_button(row, prefs.default_override == id ? "默认*" : "默认", 292, 5, 78,
                                    on_default, (void *)(uintptr_t)id);
        if (prefs.default_override == id) {
            lv_obj_set_style_bg_color(def, lv_color_hex(COL_ACCENT), 0);
        }
    }
    lv_obj_t *sound = make_button(scr, prefs.sound_enabled ? "提示音开" : "提示音关", 89, 290, 288,
                                  on_sound, NULL);
    lv_obj_set_style_bg_color(sound, lv_color_hex(0x1a222c), 0);
    lv_obj_t *back = make_button(scr, "返回", 143, 356, 180, on_back, NULL);
    lv_obj_set_style_bg_color(back, lv_color_hex(COL_ACCENT), 0);
}
