idf_component_register(SRCS "Wifi.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash Medicine esp_http_server)
