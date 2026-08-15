idf_component_register(
    SRCS "src/ui.c" "src/screen_home_view.c" "src/screen_settings_view.c"
         "src/fonts/font_cn_16.c" "src/fonts/font_cn_22.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src" "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES badge board
    PRIV_REQUIRES lvgl__lvgl esp_timer
)

# Version string for settings page
set(APP_VERSION "0.1.0")
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    OUTPUT_VARIABLE APP_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT APP_GIT_HASH)
    set(APP_GIT_HASH "unknown")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/app_version.h"
"#pragma once\n#define APP_VERSION \"${APP_VERSION}\"\n#define APP_GIT_HASH \"${APP_GIT_HASH}\"\n"
)
