idf_component_register(SRCS "ble_driver.c" "my_gpio.c"
                    INCLUDE_DIRS "include"
                    REQUIRES my_gpio my_uart nvs_flash)
