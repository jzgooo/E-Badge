#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t badge_init(void);

const char *badge_content_get_title(void);
const char *badge_content_get_subtitle(void);

#ifdef __cplusplus
}
#endif
