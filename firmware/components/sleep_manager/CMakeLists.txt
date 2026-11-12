idf_component_register(SRCS "sleep_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES my_uart my_gpio gc9a01 AXP2101 TMP117 VEML7700 MAX30102 BMI270 DRV2605L ble_driver lvgl_port 
)