# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components" 
                         "${CMAKE_CURRENT_LIST_DIR}/peripherals" 
                         "${CMAKE_CURRENT_LIST_DIR}/lvgl_driver")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Smart_Watch_In_C_ESP-IDF)
