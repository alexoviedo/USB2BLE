idf_component_register(SRCS "src/usb_host_adapter.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES charm_ports charm_contracts usb espressif__usb_host_hid
                       PRIV_REQUIRES esp_timer charm_test_support)
