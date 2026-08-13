idf_component_register(
    SRCS "src/ble.c"
    INCLUDE_DIRS "include"
    REQUIRES badge
    PRIV_REQUIRES bt nvs_flash
)
