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
)

list(APPEND inc_list
    ${COMPONENT_DIR}/include
    ${COMPONENT_DIR}/include/link
    ${COMPONENT_DIR}/include/peripheral
    ${COMPONENT_DIR}/include/protocol
    ${COMPONENT_DIR}/misc
)

list (APPEND src_list
    ${COMPONENT_DIR}/src/nvs_wrapper.cpp
    ${COMPONENT_DIR}/src/firmware_wrapper.cpp
    ${COMPONENT_DIR}/src/link/wifi_wrapper.cpp
    ${COMPONENT_DIR}/src/link/gattc_wrapper.cpp
    ${COMPONENT_DIR}/src/link/gatts_wrapper.cpp
    ${COMPONENT_DIR}/src/link/gatts_table_wrapper.cpp
    ${COMPONENT_DIR}/src/peripheral/uart_wrapper.cpp
    ${COMPONENT_DIR}/src/peripheral/pwm_wrapper.cpp
    ${COMPONENT_DIR}/src/protocol/socket_wrapper.cpp
)
