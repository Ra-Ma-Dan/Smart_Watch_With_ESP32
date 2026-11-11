idf_component_register(SRCS "ota_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES my_i2c my_uart esp_wifi sleep_manager DRV2605L lvgl_port nvs_flash)
