#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Stable UI API (maintained on main). Screens live in src/; keep ui_start(). */
esp_err_t ui_start(void);

#ifdef __cplusplus
}
#endif
