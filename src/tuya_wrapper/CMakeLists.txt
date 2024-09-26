idf_component_register(SRCS "tuya_wrapper.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        tuya-iot-core-sdk
                        wifi_wrapper
                        )
