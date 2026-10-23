idf_component_register(SRCS "net.c" "input_robot_lib.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_wifi nvs_flash esp_netif esp_event esp_driver_ledc)
