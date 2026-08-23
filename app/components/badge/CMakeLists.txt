idf_component_register(
    SRCS "src/badge_quota.c" "src/badge_dashboard.c" "src/badge_content.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash json
    PRIV_REQUIRES esp_timer
)
