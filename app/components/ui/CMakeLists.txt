idf_component_register(
    SRCS "src/ui.c" "src/screen_home.c" "src/screen_settings.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES badge board
    PRIV_REQUIRES lvgl__lvgl
)
