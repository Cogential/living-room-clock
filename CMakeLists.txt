cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# LVGL and the BSP headers trip a few warnings that are not ours to fix.
add_compile_options(-Wno-format)
add_compile_options(-Wno-ignored-qualifiers)
add_compile_options(-Wno-unused-function)

project(living_room_clock)

# doom1.wad is shareware and freely redistributable. It is built into the
# "storage" partition image and flashed alongside the app.
spiffs_create_partition_image(storage spiffs FLASH_IN_PROJECT)
