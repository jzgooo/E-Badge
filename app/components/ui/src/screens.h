#pragma once

#include <stdbool.h>

void screen_home_show(void);
void screen_home_refresh(void);
bool screen_home_is_active(void);
/** 从主页进入设置（停主页定时器）。非主页时无操作。 */
void screen_open_settings(void);
void screen_settings_show(void);
