idf_component_register(
    SRCS "src/board.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash
    PRIV_REQUIRES waveshare__esp32_s3_touch_amoled_1_75c lvgl
)
