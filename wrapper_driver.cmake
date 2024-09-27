list (APPEND dependencies
esp_wifi
nvs_flash
# bt
# mqtt
# esp_https_ota
# utility		#os，dev_desc
)

list (APPEND src_list
    ${COMPONENT_DIR}/src/link/wifi_wrapper.cpp
    ${COMPONENT_DIR}/src/peripheral/nvs_wrapper.cpp
)
