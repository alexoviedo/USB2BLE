idf_component_register(SRCS "src/esp_timer_mock.cpp" "src/nvs_mock.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES charm_contracts charm_ports)
