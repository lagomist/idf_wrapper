idf_component_register(SRCS "blufi_wrapper.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        wifi_wrapper
                        ble_gatt_wrapper
                        json
                        )
