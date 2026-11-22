cmake_minimum_required(VERSION 3.21)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")

set(APP_NAME "Atelier")
set(APP_USER_NAME "Atelier")
set(APP_FILE_NAME "Atelier")
set(APP_TYPE "Clockface")
set(DEV_ID "UNA")
set(APP_ID "0728DB9D670DEA0F")
# Pinned rather than taken from apps-v* git tags; bump it for every store upload
set(BUILD_VERSION "0.1.0")
set(RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../Resources")
set(OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../Output")
set(LIBS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../Libs")
set(GUI_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../LVGL-GUI")

if("$ENV{UNA_SDK}" STREQUAL "")
    message(FATAL_ERROR "UNA_SDK environment variable is not set. Point it at the SDK root.")
endif()

include($ENV{UNA_SDK}/cmake/una-app.cmake)
include($ENV{UNA_SDK}/cmake/una-sdk.cmake)
include(${LIBS_PATH}/libs.cmake)
include(${GUI_PATH}/lvgl-gui.cmake)

project(${APP_NAME})

una_app_setup_version(BUILD_VERSION ${CMAKE_CURRENT_SOURCE_DIR})

set(SERVICE_SOURCES
    ${LIBS_SOURCES}
    ${UNA_SDK_SOURCES_COMMON}
    ${UNA_SDK_SOURCES_SERVICE}
)
set(SERVICE_INCLUDE_DIRS
    ${LIBS_INCLUDE_DIRS}
    ${UNA_SDK_INCLUDE_DIRS_COMMON}
    ${UNA_SDK_INCLUDE_DIRS_SERVICE}
)
una_app_build_service(${APP_NAME}Service.elf)

set(GUI_SOURCES
    ${GUI_APP_SOURCES}
    ${UNA_SDK_SOURCES_COMMON}
    ${UNA_SDK_SOURCES_GUI_LVGL}
)
set(GUI_INCLUDE_DIRS
    ${LIBS_INCLUDE_DIRS}
    ${UNA_SDK_INCLUDE_DIRS_COMMON}
    ${UNA_SDK_INCLUDE_DIRS_GUI_LVGL}
    ${GUI_APP_INCLUDE_DIRS}
)
# Measured peak is about 6 KB (painter tasks are dispatched at once under
# LV_OS_NONE; rotated hands allocate too). Written as a plain number because
# CMake drops parenthesised -D values.
set(GUI_COMPILE_DEFINITIONS
    ${UNA_SDK_DEFINES_GUI_LVGL}
    LV_MEM_SIZE=24576
)
set(GUI_STACK_SIZE "24*1024")
una_app_build_gui(${APP_NAME}GUI.elf)

una_app_build_app()
