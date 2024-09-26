idf_component_register(SRCS "http_ota_wrapper.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        wifi_wrapper
                        esp_https_ota
                        app_update
                        )
