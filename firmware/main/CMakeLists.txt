idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES    my_gpio my_uart my_spi my_i2c ble_driver gc9a01 axp2101 max17048 
                                PFC85063a TMP117 VEML7700 MAC30102 BMI270 CST816s DRV2605l)