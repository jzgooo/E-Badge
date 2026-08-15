idf_component_register(
    SRCS "src/board.c" "src/board_pmu.cpp"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash
    PRIV_REQUIRES waveshare__esp32_s3_touch_amoled_1_75c lvgl XPowersLib driver
)

# XPowersLib 通过宏选择 AXP2101
target_compile_definitions(${COMPONENT_LIB} PRIVATE XPOWERS_CHIP_AXP2101)
