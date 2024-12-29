list (APPEND dependencies
    esp_wifi
    nvs_flash
    bt
    driver
    esp_adc
    app_update
    esp_app_format
    mqtt
    esp_https_ota
    json
    fatfs
)

list(APPEND priv_inc_list
    ${COMPONENT_DIR}/config
)

list(APPEND inc_list
    ${COMPONENT_DIR}/include
    ${COMPONENT_DIR}/include/link
    ${COMPONENT_DIR}/include/peripheral
    ${COMPONENT_DIR}/include/protocol
    ${COMPONENT_DIR}/include/misc
    ${COMPONENT_DIR}/include/utility
)

list (APPEND src_list
    ${COMPONENT_DIR}/src/misc/json_wrapper.cpp
    ${COMPONENT_DIR}/src/misc/firmware_wrapper.cpp
    ${COMPONENT_DIR}/src/misc/ota_wrapper.cpp
    ${COMPONENT_DIR}/src/misc/fs_wrapper.cpp
    ${COMPONENT_DIR}/src/link/wifi_wrapper.cpp
    ${COMPONENT_DIR}/src/link/gattc_wrapper.cpp
    ${COMPONENT_DIR}/src/link/gatts_wrapper.cpp
    ${COMPONENT_DIR}/src/link/gatts_table_wrapper.cpp
    ${COMPONENT_DIR}/src/peripheral/nvs_wrapper.cpp
    ${COMPONENT_DIR}/src/peripheral/uart_wrapper.cpp
    ${COMPONENT_DIR}/src/peripheral/pwm_wrapper.cpp
    ${COMPONENT_DIR}/src/protocol/socket_wrapper.cpp
    ${COMPONENT_DIR}/src/protocol/mqtt_wrapper.cpp
    ${COMPONENT_DIR}/src/utility/utils_wrapper.cpp
    ${COMPONENT_DIR}/src/utility/shell_wrapper.cpp
    ${COMPONENT_DIR}/src/utility/time_wrapper.cpp
    ${COMPONENT_DIR}/src/utility/os_wrapper.cpp
)
