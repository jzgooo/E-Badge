#pragma once

#include "lvgl.h"

/*
 * 精简中文字库（黑体），只含界面用字 + ASCII。
 * 增字后用 lv_font_conv 重生成 font_cn_16.c / font_cn_22.c。
 */
extern const lv_font_t font_cn_16;
extern const lv_font_t font_cn_22;
