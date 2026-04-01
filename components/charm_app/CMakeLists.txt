idf_component_register(SRCS "src/app_bootstrap.cpp" "src/config_activation.cpp" "src/config_transport_service.cpp" "src/config_transport_runtime_adapter.cpp" "src/runtime_data_plane.cpp" "src/startup_storage_lifecycle.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES charm_core charm_ports charm_platform_ble charm_platform_usb charm_platform_time charm_platform_storage driver)
