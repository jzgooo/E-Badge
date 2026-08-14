#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Stable audio API (maintained on main). */
esp_err_t audio_start(void);

#ifdef __cplusplus
}
#endif
