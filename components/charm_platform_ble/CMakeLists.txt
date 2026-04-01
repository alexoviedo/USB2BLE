idf_component_register(
    SRCS "src/ble_transport_adapter.cpp"
    INCLUDE_DIRS "include"
    REQUIRES charm_contracts charm_ports bt
)
