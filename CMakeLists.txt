cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# LVGL and the BSP headers trip a few warnings that are not ours to fix.
add_compile_options(-Wno-format)
add_compile_options(-Wno-ignored-qualifiers)
add_compile_options(-Wno-unused-function)

project(living_room_clock)

# Everything in spiffs/ is built into the "storage" partition image and flashed
# with `idf.py flash`. The repo ships it empty: drop doom1.wad in to get DOOM
# (see README). Created here so a fresh clone builds without it.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/spiffs)
spiffs_create_partition_image(storage spiffs FLASH_IN_PROJECT)
