set(SRCS
    "main.cpp"
    "esp_hal.cpp"
    "ssd1306.cpp"
    "display_manager.cpp"
    "lora_transport.cpp"
    "meshtastic_ble.cpp"
    "meshtastic_encoder.cpp"
    "mesh/mesh_protocol.cpp"
    "sensors/gps_service.cpp"
    "storage/message_storage.cpp"
    "generated/google/protobuf/descriptor.pb.c"
    "generated/meshtastic/admin.pb.c"
    "generated/meshtastic/channel.pb.c"
    "generated/meshtastic/config.pb.c"
    "generated/meshtastic/connection_status.pb.c"
    "generated/meshtastic/device_ui.pb.c"
    "generated/meshtastic/mesh.pb.c"
    "generated/meshtastic/module_config.pb.c"
    "generated/meshtastic/mqtt.pb.c"
    "generated/meshtastic/nanopb.pb.c"
    "generated/meshtastic/portnums.pb.c"
    "generated/meshtastic/telemetry.pb.c"
    "generated/meshtastic/xmodem.pb.c"
)

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "." "generated"
    REQUIRES
        driver
        esp_timer
        nvs_flash
        bt
        spi_flash
        spiffs
        esp_event
        vfs
        Nanopb
        RadioLib
        TinyGPSPlus
)

# Nanopb build flags
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    PB_ENABLE_MALLOC=1
    PB_FIELD_32BIT=1
)
