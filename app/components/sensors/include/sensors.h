#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Stable sensors API (maintained on main). */
esp_err_t sensors_start(void);

#ifdef __cplusplus
}
#endif
