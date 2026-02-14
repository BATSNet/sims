# Main component CMakeLists.txt
# SIMS-SMART ESP-IDF Port

set(SRCS
    "main.cpp"
    "network/wifi_service.cpp"
    "network/http_client.cpp"
    "storage/nvs_storage.cpp"
    "sensors/gps_service.cpp"
    "sensors/camera_service.cpp"
    "sensors/audio_service.cpp"
    "voice/wake_word_service.cpp"
    "voice/command_parser.cpp"
    "led/led_feedback.cpp"
    "power/power_manager.cpp"
)

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_wifi
        esp_netif
        esp_event
        esp_http_client
        nvs_flash
        spiffs
        esp32-camera
        app_update
        mbedtls
        json
        esp_adc
        esp_pm
        spi_flash
        TinyGPSPlus
        esp-sr
        libwebp
)

# C++ standard for all source files
set_source_files_properties(${SRCS}
    PROPERTIES COMPILE_FLAGS "-std=gnu++17"
)
