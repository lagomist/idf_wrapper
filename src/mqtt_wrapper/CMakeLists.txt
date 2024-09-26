idf_component_register(SRCS "mqtt_wrapper.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        mqtt
                        wifi_wrapper
                        )
