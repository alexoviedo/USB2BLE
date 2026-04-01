idf_component_register(SRCS "src/config_store_nvs.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES charm_contracts charm_ports nvs_flash)
