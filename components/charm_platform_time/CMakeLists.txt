idf_component_register(SRCS "src/time_port_esp_idf.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES charm_contracts charm_ports esp_timer)
