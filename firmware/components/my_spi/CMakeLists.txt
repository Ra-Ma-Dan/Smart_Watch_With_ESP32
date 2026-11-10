idf_component_register(SRCS "my_spi.c"
                    INCLUDE_DIRS "include"
                    REQUIRES my_gpio esp_driver_spi)
