idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES    my_gpio my_uart my_spi my_i2c ble_driver gc9a01 AXP2101 MAX17048 
                                PFC85063A TMP117 VEML7700 MAX30102 BMI270 CST816S DRV2605L)