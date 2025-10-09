# ----------------------------------------------------------------------------
# General settings/includes
# ----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
if (NOT FREERTOS_KERNEL_PATH AND NOT DEFINED ENV{FREERTOS_KERNEL_PATH})
        message(FATAL_ERROR "Missing FreeRTOS kernel source path. Set by declaring environment variable FREERTOS_KERNEL_PATH or add -DFREERTOS_KERNEL_PATH=<path> to the cmake call")
endif()
if (NOT FREERTOS_KERNEL_PATH AND DEFINED ENV{FREERTOS_KERNEL_PATH})
        set(FREERTOS_KERNEL_PATH ENV{FREERTOS_KERNEL_PATH})
endif()
if (NOT LIBMODBUS_STATIC_PATH AND NOT DEFINED ENV{LIBMODBUS_STATIC_PATH})
        message(FATAL_ERROR "Missing LIBMODBUS_STATIC_PATH source path. Set by declaring environment variable LIBMODBUS_STATIC_PATH or add -DLIBMODBUS_STATIC_PATH=<path> to the cmake call. Repo link: https://github.com/Lachei/libmodbus-static.git")
endif()
if (NOT LIBMODBUS_STATIC_PATH AND DEFINED ENV{LIBMODBUS_STATIC_PATH})
        set(LIBMODBUS_STATIC_PATH ENV{LIBMODBUS_STATIC_PATH})
endif()

if (NOT PICO_BOARD)
        message("No board set, defaulting to pico_w")
        set(PICO_BOARD pico_w)
endif()

include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)
include(FreeRTOS_Kernel_import.cmake)
include(cmake/html_pages_library.cmake)
add_subdirectory(${LIBMODBUS_STATIC_PATH} libmodbus-static)

add_compile_options(-Wall)

project(modbus-meter C CXX)

pico_sdk_init()

set(CMAKE_BUILD_TYPE "MinSizeRel")

if (NOT PICO_CYW43_SUPPORTED)
        message(FATAL_ERROR "Pico w not supported by your sdk. Make sure to have PICO_CYW43_SUPPORTED set by your sdk installation")
endif()


# ----------------------------------------------------------------------------
# Website content
# ----------------------------------------------------------------------------
add_html_pages_library(NAME ${PROJECT_NAME}-html SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/404.html
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/index.html
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/style.css
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/internet.html
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/overview.html
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/settings.html
)


# ----------------------------------------------------------------------------
# Executable
# ----------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
        src/main.cpp
        src/dhcpserver.c
        src/dnsserver.c
        src/log_storage.cpp
        src/ntp_client.cpp
        src/modbus_server_client.cpp
        src/http_solarapi.cpp
)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/modbus_layouts
)
target_link_libraries(${PROJECT_NAME}
        pico_stdlib
        pico_mbedtls
        hardware_flash
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_mdns
        FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
        ${PROJECT_NAME}-html
        libmodbus-static
)
pico_add_extra_outputs(${PROJECT_NAME})
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

