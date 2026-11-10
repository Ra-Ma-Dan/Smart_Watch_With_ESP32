idf_component_register(SRCS "my_uart.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_flash esp_driver_uart)
