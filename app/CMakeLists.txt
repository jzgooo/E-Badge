cmake_minimum_required(VERSION 3.16)

# 复用官方示例里的 XPowersLib（AXP2101），避免拷贝进 app/
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../examples/ESP-IDF-v5.5/01_AXP2101/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-Wno-format)
project(e_badge)
