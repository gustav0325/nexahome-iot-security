idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
  PRIV_REQUIRES nvs_flash esp_netif esp_eth mqtt esp_http_client esp_http_server app_update esp_timer driver esp_driver_uart esp_event bootloader_support efuse)
