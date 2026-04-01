# Unit tests for pure-core modules.
# S-008 adds device-registry coverage using a standalone compile/run path.
# S-009 adds hid-semantic-model coverage using a standalone compile/run path.
# S-010 adds decode-plan builder coverage using a standalone compile/run path.
# S-011 adds hid report decoder coverage using a standalone compile/run path.
# S-012 adds logical gamepad state container coverage using a standalone compile/run path.
# S-013 adds mapping bundle validator coverage using a standalone compile/run path.
# S-014 adds mapping engine coverage using a standalone compile/run path.

cmake_minimum_required(VERSION 3.16)
project(CharmUnitTests)

# We want C++20 for all pure-core logic
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GTest bootstrap: prefer system package, but provide repo-local fallback
# so clean environments do not require manual preinstallation.
option(CHARM_FETCH_GTEST "Fetch GoogleTest when no system package is found" ON)
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  if(NOT CHARM_FETCH_GTEST)
    message(FATAL_ERROR "GTest not found and CHARM_FETCH_GTEST=OFF")
  endif()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  # On some platforms this avoids overriding parent compiler/linker flags.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# The directories for contracts and ports (just headers)
set(CONTRACTS_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../components/charm_contracts/include")
set(PORTS_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../components/charm_ports/include")
set(TEST_SUPPORT_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../components/charm_test_support/include")
set(CORE_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../components/charm_core/include")
set(APP_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../components/charm_app/include")

# Build pure core test support as an OBJECT library since it has no main
add_library(charm_core_obj OBJECT
    "../../components/charm_core/src/device_registry.cpp"
    "../../components/charm_core/src/hid_semantic_model.cpp"
    "../../components/charm_core/src/decode_plan.cpp"
    "../../components/charm_core/src/hid_decoder.cpp"
    "../../components/charm_core/src/logical_state.cpp"
    "../../components/charm_core/src/mapping_bundle.cpp"
    "../../components/charm_core/src/mapping_engine.cpp"
    "../../components/charm_core/src/profile_manager.cpp"
    "../../components/charm_core/src/profile_generic_gamepad_encoder.cpp"
    "../../components/charm_core/src/supervisor.cpp"
    "../../components/charm_core/src/recovery_policy.cpp"
    "../../components/charm_platform_time/src/time_port_esp_idf.cpp"
    "../../components/charm_test_support/src/esp_timer_mock.cpp"
    "../../components/charm_platform_storage/src/config_store_nvs.cpp"
    "../../components/charm_test_support/src/nvs_mock.cpp"
)
target_include_directories(charm_core_obj PUBLIC
    ${CONTRACTS_INC}
    ${PORTS_INC}
    ${CORE_INC}
    "${CMAKE_CURRENT_SOURCE_DIR}/../../components/charm_platform_time/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../components/charm_platform_storage/include"
    ${TEST_SUPPORT_INC}
)

# S-008 Device Registry
add_executable(test_device_registry test_device_registry.cpp)
target_include_directories(test_device_registry PRIVATE ${TEST_SUPPORT_INC} ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_device_registry PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

# S-009 HID Semantic Model
add_executable(test_hid_semantic_model test_hid_semantic_model.cpp)
target_include_directories(test_hid_semantic_model PRIVATE ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_hid_semantic_model PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

# S-010 Decode Plan Builder
add_executable(test_decode_plan test_decode_plan.cpp)
target_include_directories(test_decode_plan PRIVATE ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_decode_plan PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

# S-011 HID Report Decoder
add_executable(test_hid_decoder test_hid_decoder.cpp)
target_include_directories(test_hid_decoder PRIVATE ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_hid_decoder PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

# S-012 Logical Gamepad State
add_executable(test_logical_state test_logical_state.cpp)
target_include_directories(test_logical_state PRIVATE ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_logical_state PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

# S-013 Mapping Bundle
add_executable(test_mapping_bundle test_mapping_bundle.cpp)
target_include_directories(test_mapping_bundle PRIVATE ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_mapping_bundle PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

# S-014 Mapping Engine
add_executable(test_mapping_engine test_mapping_engine.cpp)
target_include_directories(test_mapping_engine PRIVATE ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_mapping_engine PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

# S-015 Profile Manager and Generic Gamepad Encoder
add_executable(test_profile_manager test_profile_manager.cpp)
target_include_directories(test_profile_manager PRIVATE ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_profile_manager PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

add_executable(test_profile_generic_gamepad_encoder test_profile_generic_gamepad_encoder.cpp)
target_include_directories(test_profile_generic_gamepad_encoder PRIVATE ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_profile_generic_gamepad_encoder PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

# S-016 Supervisor
add_executable(test_supervisor test_supervisor.cpp)
target_include_directories(test_supervisor PRIVATE ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_supervisor PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

# S-023 Recovery Policy
add_executable(test_recovery_policy test_recovery_policy.cpp)
target_include_directories(test_recovery_policy PRIVATE ${CONTRACTS_INC} ${CORE_INC})
target_link_libraries(test_recovery_policy PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)

# Register tests
include(CTest)
add_test(NAME DeviceRegistryTest COMMAND test_device_registry)
add_test(NAME HidSemanticModelTest COMMAND test_hid_semantic_model)
add_test(NAME DecodePlanTest COMMAND test_decode_plan)
add_test(NAME HidDecoderTest COMMAND test_hid_decoder)
add_test(NAME LogicalStateTest COMMAND test_logical_state)
add_test(NAME MappingBundleTest COMMAND test_mapping_bundle)
add_test(NAME MappingEngineTest COMMAND test_mapping_engine)
add_test(NAME ProfileManagerTest COMMAND test_profile_manager)
add_test(NAME ProfileGenericGamepadEncoderTest COMMAND test_profile_generic_gamepad_encoder)
add_test(NAME SupervisorTest COMMAND test_supervisor)
add_test(NAME RecoveryPolicyTest COMMAND test_recovery_policy)

# S-019 USB Host Adapter
add_executable(test_usb_host_adapter test_usb_host_adapter.cpp)
target_include_directories(test_usb_host_adapter PRIVATE ${CONTRACTS_INC} ${PORTS_INC} "${CMAKE_CURRENT_SOURCE_DIR}/../../components/charm_platform_usb/include")
# We need to link the adapter logic and obj library
target_sources(test_usb_host_adapter PRIVATE "../../components/charm_platform_usb/src/usb_host_adapter.cpp")
target_link_libraries(test_usb_host_adapter PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)
add_test(NAME UsbHostAdapterTest COMMAND test_usb_host_adapter)

# S-020 BLE Transport Adapter
add_executable(test_ble_transport_adapter test_ble_transport_adapter.cpp)
target_include_directories(test_ble_transport_adapter PRIVATE ${CONTRACTS_INC} ${PORTS_INC} "${CMAKE_CURRENT_SOURCE_DIR}/../../components/charm_platform_ble/include")
target_sources(test_ble_transport_adapter PRIVATE "../../components/charm_platform_ble/src/ble_transport_adapter.cpp")
target_link_libraries(test_ble_transport_adapter PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)
add_test(NAME BleTransportAdapterTest COMMAND test_ble_transport_adapter)

# S-022 Config Activation
add_executable(test_config_activation test_config_activation.cpp)
target_include_directories(test_config_activation PRIVATE ${CONTRACTS_INC} ${PORTS_INC} ${TEST_SUPPORT_INC} "${CMAKE_CURRENT_SOURCE_DIR}/../../components/charm_app/include")
target_sources(test_config_activation PRIVATE "../../components/charm_app/src/config_activation.cpp")
target_link_libraries(test_config_activation PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)
add_test(NAME ConfigActivationTest COMMAND test_config_activation)

# CFG-002 Config Transport Service
add_executable(test_config_transport_service test_config_transport_service.cpp)
target_include_directories(test_config_transport_service PRIVATE ${CONTRACTS_INC} ${PORTS_INC} ${TEST_SUPPORT_INC} ${APP_INC})
target_sources(test_config_transport_service PRIVATE "../../components/charm_app/src/config_transport_service.cpp")
target_link_libraries(test_config_transport_service PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)
add_test(NAME ConfigTransportServiceTest COMMAND test_config_transport_service)


# VS-01 Runtime Data Plane
add_executable(test_runtime_data_plane test_runtime_data_plane.cpp)
target_include_directories(test_runtime_data_plane PRIVATE ${CONTRACTS_INC} ${PORTS_INC} ${CORE_INC} ${APP_INC} ${TEST_SUPPORT_INC})
target_sources(test_runtime_data_plane PRIVATE "../../components/charm_app/src/runtime_data_plane.cpp")
target_link_libraries(test_runtime_data_plane PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)
add_test(NAME RuntimeDataPlaneTest COMMAND test_runtime_data_plane)


# VS-02 Config Transport Runtime Adapter
add_executable(test_config_transport_runtime_adapter test_config_transport_runtime_adapter.cpp)
target_include_directories(test_config_transport_runtime_adapter PRIVATE ${CONTRACTS_INC} ${PORTS_INC} ${TEST_SUPPORT_INC} ${APP_INC})
target_sources(test_config_transport_runtime_adapter PRIVATE "../../components/charm_app/src/config_transport_service.cpp" "../../components/charm_app/src/config_transport_runtime_adapter.cpp")
target_link_libraries(test_config_transport_runtime_adapter PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)
add_test(NAME ConfigTransportRuntimeAdapterTest COMMAND test_config_transport_runtime_adapter)

# VS-04 Startup Storage Lifecycle
add_executable(test_startup_storage_lifecycle test_startup_storage_lifecycle.cpp)
target_include_directories(test_startup_storage_lifecycle PRIVATE ${CONTRACTS_INC} ${PORTS_INC} ${TEST_SUPPORT_INC} ${CORE_INC} ${APP_INC})
target_sources(test_startup_storage_lifecycle PRIVATE "../../components/charm_app/src/startup_storage_lifecycle.cpp")
target_link_libraries(test_startup_storage_lifecycle PRIVATE charm_core_obj GTest::gtest_main GTest::gtest)
add_test(NAME StartupStorageLifecycleTest COMMAND test_startup_storage_lifecycle)
