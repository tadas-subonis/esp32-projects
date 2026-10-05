idf_component_register(
    SRCS
        "main.cpp"
        "console.cpp"
        "device_hal.cpp"
        "ili9488.cpp"
        "game_app.cpp"
        "net_client.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../game/src/world.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../game/src/physics.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../game/src/match.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../game/src/bot.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../game/src/render.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../game/src/log.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../shared/protocol/codec.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../shared/protocol/wire.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../shared/protocol/link.cpp"
    INCLUDE_DIRS
        "."
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/../../game/include"
        "${CMAKE_CURRENT_LIST_DIR}/../../shared/protocol"
        "${CMAKE_CURRENT_LIST_DIR}/../../shared/net"
    REQUIRES
        nvs_flash
        esp_app_format
        esp_timer
        esp_lcd
        esp_netif
        esp_event
        driver
        console
        esp_driver_uart
        heap
        lwip
)

set(_wifi_ssid "")
set(_wifi_pass "")
set(_server_host "192.168.8.229")
set(_server_port "7420")
set(_local_env "${CMAKE_CURRENT_LIST_DIR}/../../.local.env")
if(EXISTS "${_local_env}")
    file(STRINGS "${_local_env}" _env_lines)
    foreach(_line IN LISTS _env_lines)
        if(_line MATCHES "^WIFI_SSID=(.*)$")
            set(_wifi_ssid "${CMAKE_MATCH_1}")
        elseif(_line MATCHES "^WIFI_PASS=(.*)$")
            set(_wifi_pass "${CMAKE_MATCH_1}")
        elseif(_line MATCHES "^SERVER_HOST=(.*)$")
            set(_server_host "${CMAKE_MATCH_1}")
        elseif(_line MATCHES "^SERVER_PORT=(.*)$")
            set(_server_port "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endif()
string(STRIP "${_wifi_ssid}" ARTILLERY_WIFI_SSID)
string(STRIP "${_wifi_pass}" ARTILLERY_WIFI_PASS)
string(STRIP "${_server_host}" ARTILLERY_SERVER_HOST)
string(STRIP "${_server_port}" ARTILLERY_SERVER_PORT)
string(REPLACE "\r" "" ARTILLERY_WIFI_SSID "${ARTILLERY_WIFI_SSID}")
string(REPLACE "\r" "" ARTILLERY_WIFI_PASS "${ARTILLERY_WIFI_PASS}")
string(REPLACE "\r" "" ARTILLERY_SERVER_HOST "${ARTILLERY_SERVER_HOST}")
string(REPLACE "\r" "" ARTILLERY_SERVER_PORT "${ARTILLERY_SERVER_PORT}")
if(NOT ARTILLERY_SERVER_PORT)
    set(ARTILLERY_SERVER_PORT "7420")
endif()
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/secrets.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/secrets.h"
    @ONLY
)
