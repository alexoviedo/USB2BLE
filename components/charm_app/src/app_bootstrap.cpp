#include "charm/app/app_bootstrap.hpp"
#include "charm/app/config_activation.hpp"
#include "charm/app/config_transport_runtime_adapter.hpp"
#include "charm/app/config_transport_service.hpp"
#include "charm/app/runtime_data_plane.hpp"
#include "charm/app/startup_storage_lifecycle.hpp"

#include "charm/core/decode_plan.hpp"
#include "charm/core/device_registry.hpp"
#include "charm/core/hid_decoder.hpp"
#include "charm/core/hid_semantic_model.hpp"
#include "charm/core/logical_state.hpp"
#include "charm/core/mapping_engine.hpp"
#include "charm/core/profile_manager.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/core/recovery_policy.hpp"
#include "charm/platform/ble_transport_adapter.hpp"
#include "charm/platform/time_port_esp_idf.hpp"
#include "charm/platform/usb_host_adapter.hpp"
#include "charm/platform/config_store_nvs.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef CHARM_CFG_TRANSPORT_BINDING_AUDIT
#define CHARM_CFG_TRANSPORT_BINDING_AUDIT 0
#endif

#if __has_include("esp_log.h")
#include "esp_log.h"
#define CHARM_CFG_BINDING_LOGW(fmt, ...) \
  ESP_LOGW("cfg_transport_runtime", fmt, ##__VA_ARGS__)
#if CHARM_CFG_TRANSPORT_BINDING_AUDIT
#define CHARM_CFG_BINDING_AUDIT_LOGI(fmt, ...) \
  ESP_LOGI("cfg_transport_audit", fmt, ##__VA_ARGS__)
#else
#define CHARM_CFG_BINDING_AUDIT_LOGI(...) ((void)0)
#endif
#else
#define CHARM_CFG_BINDING_LOGW(fmt, ...) \
  std::fprintf(stderr, "[cfg_transport_runtime][W] " fmt "\n", ##__VA_ARGS__)
#if CHARM_CFG_TRANSPORT_BINDING_AUDIT
#define CHARM_CFG_BINDING_AUDIT_LOGI(fmt, ...) \
  std::fprintf(stderr, "[cfg_transport_audit][I] " fmt "\n", ##__VA_ARGS__)
#else
#define CHARM_CFG_BINDING_AUDIT_LOGI(...) ((void)0)
#endif
#endif

#if __has_include("driver/uart.h") && __has_include("freertos/FreeRTOS.h") && \
    __has_include("freertos/task.h")
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define CHARM_CFG_UART_RUNTIME_AVAILABLE 1
#else
#define CHARM_CFG_UART_RUNTIME_AVAILABLE 0
#endif

namespace charm::app {

// Static instances for simple thin wiring. In a fully mature system
// these might be managed by a more formal context container.
static charm::platform::TimePortEspIdf time_port;
static charm::core::CanonicalLogicalStateStore state_store({1});
static charm::core::DefaultMappingEngine mapping_engine(state_store);
static charm::core::CanonicalProfileManager profile_manager;
static charm::core::DefaultMappingBundleValidator mapping_bundle_validator;
static charm::core::DefaultMappingBundleLoader mapping_bundle_loader(
    &mapping_bundle_validator);
static charm::core::DefaultConfigCompiler config_compiler;
static charm::core::InMemoryDeviceRegistry device_registry;
static charm::core::DefaultHidDescriptorParser descriptor_parser;
static charm::core::DefaultDecodePlanBuilder decode_plan_builder;
static charm::core::DefaultHidDecoder hid_decoder;
static charm::platform::UsbHostAdapter usb_host;
static charm::platform::BleTransportAdapter ble_transport;
static charm::platform::ConfigStoreNvs config_store;
static charm::core::DefaultSupervisor supervisor;
static ConfigTransportService config_transport_service(
    config_store, config_compiler, mapping_bundle_loader, supervisor);
static ConfigTransportRuntimeAdapter config_transport_runtime_adapter(config_transport_service);
static charm::core::DefaultRecoveryPolicy recovery_policy(supervisor);
static RuntimeDataPlane runtime_data_plane(usb_host, ble_transport, device_registry,
                                           descriptor_parser, decode_plan_builder,
                                           hid_decoder, mapping_engine,
                                           mapping_bundle_loader,
                                           profile_manager, supervisor);

namespace {

#if CHARM_CFG_UART_RUNTIME_AVAILABLE
constexpr uart_port_t kConfigUartPort = UART_NUM_0;
constexpr int kConfigUartBaud = 115200;
constexpr std::size_t kConfigRxBufferBytes = 2048;
constexpr std::size_t kConfigTxBufferBytes = 2048;
constexpr std::size_t kConfigReadChunk = 128;

class UartSerialWriter final : public SerialTransportWriter {
 public:
  explicit UartSerialWriter(uart_port_t port) : port_(port) {}
  bool Write(const std::uint8_t* bytes, std::size_t size) override {
    if (bytes == nullptr || size == 0) {
      return false;
    }
    const int written =
        uart_write_bytes(port_, reinterpret_cast<const char*>(bytes), size);
    return written == static_cast<int>(size);
  }

 private:
  uart_port_t port_;
};

static bool config_runtime_running = false;
static TaskHandle_t config_runtime_task = nullptr;

void ConfigTransportRuntimeTask(void* arg) {
  auto* adapter = static_cast<ConfigTransportRuntimeAdapter*>(arg);
  UartSerialWriter writer(kConfigUartPort);
  std::array<std::uint8_t, kConfigReadChunk> rx{};
#if CHARM_CFG_TRANSPORT_BINDING_AUDIT
  std::uint32_t total_bytes_read = 0;
#endif
  bool write_stall_logged = false;

  CHARM_CFG_BINDING_AUDIT_LOGI("runtime started port=%d baud=%d",
                               static_cast<int>(kConfigUartPort),
                               kConfigUartBaud);

  while (config_runtime_running) {
    const int read = uart_read_bytes(kConfigUartPort, rx.data(), rx.size(),
                                     pdMS_TO_TICKS(25));
    if (read > 0) {
      adapter->ConsumeBytes(rx.data(), static_cast<std::size_t>(read));
#if CHARM_CFG_TRANSPORT_BINDING_AUDIT
      total_bytes_read += static_cast<std::uint32_t>(read);
      const auto stats = adapter->Stats();
      CHARM_CFG_BINDING_AUDIT_LOGI(
          "rx port=%d bytes_read=%d bytes_total=%u parsed_frames=%u emitted_frames=%u",
          static_cast<int>(kConfigUartPort), read, total_bytes_read,
          stats.parsed_frames, stats.emitted_frames);
#endif
    }
    while (adapter->HasPendingFrame()) {
      if (!adapter->WritePendingFrame(writer)) {
        if (!write_stall_logged) {
#if CHARM_CFG_TRANSPORT_BINDING_AUDIT
          const auto stats = adapter->Stats();
          CHARM_CFG_BINDING_LOGW(
              "tx stalled port=%d parsed_frames=%u emitted_frames=%u",
              static_cast<int>(kConfigUartPort), stats.parsed_frames,
              stats.emitted_frames);
#else
          CHARM_CFG_BINDING_LOGW("tx stalled port=%d",
                                 static_cast<int>(kConfigUartPort));
#endif
          write_stall_logged = true;
        }
        break;
      }
      write_stall_logged = false;
#if CHARM_CFG_TRANSPORT_BINDING_AUDIT
      const auto after = adapter->Stats();
      CHARM_CFG_BINDING_AUDIT_LOGI(
          "tx port=%d parsed_frames=%u emitted_frames=%u",
          static_cast<int>(kConfigUartPort), after.parsed_frames,
          after.emitted_frames);
#endif
    }
  }
  vTaskDelete(nullptr);
}

void StartConfigTransportRuntime(ConfigTransportRuntimeAdapter& adapter) {
  uart_config_t uart_cfg;
  std::memset(&uart_cfg, 0, sizeof(uart_cfg));
  uart_cfg.baud_rate = kConfigUartBaud;
  uart_cfg.data_bits = UART_DATA_8_BITS;
  uart_cfg.parity = UART_PARITY_DISABLE;
  uart_cfg.stop_bits = UART_STOP_BITS_1;
  uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_cfg.source_clk = UART_SCLK_DEFAULT;

  const int install_rc = uart_driver_install(kConfigUartPort, kConfigRxBufferBytes,
                                             kConfigTxBufferBytes, 0, nullptr, 0);
  if (install_rc != 0) {
    CHARM_CFG_BINDING_LOGW("bootstrap failed port=%d stage=driver_install rc=%d",
                           static_cast<int>(kConfigUartPort), install_rc);
    return;
  }

  const int param_rc = uart_param_config(kConfigUartPort, &uart_cfg);
  if (param_rc != 0) {
    CHARM_CFG_BINDING_LOGW("bootstrap failed port=%d stage=param_config rc=%d",
                           static_cast<int>(kConfigUartPort), param_rc);
    (void)uart_driver_delete(kConfigUartPort);
    return;
  }

  config_runtime_running = true;
  const BaseType_t task_rc =
      xTaskCreatePinnedToCore(&ConfigTransportRuntimeTask, "cfg_uart_runtime",
                              4096, &adapter, 5, &config_runtime_task,
                              tskNO_AFFINITY);
  if (task_rc != pdPASS) {
    config_runtime_running = false;
    config_runtime_task = nullptr;
    CHARM_CFG_BINDING_LOGW("bootstrap failed port=%d stage=task_create rc=%d",
                           static_cast<int>(kConfigUartPort),
                           static_cast<int>(task_rc));
    (void)uart_driver_delete(kConfigUartPort);
    return;
  }
  CHARM_CFG_BINDING_AUDIT_LOGI("bootstrap ready port=%d baud=%d",
                               static_cast<int>(kConfigUartPort),
                               kConfigUartBaud);
}
#endif

}  // namespace

void InitializeAndRun() {
  if (!InitializeStorageAndActivate(config_store, mapping_bundle_loader, supervisor,
                                    &ActivatePersistedConfig,
                                    DefaultStorageInitFns())) {
    return;
  }

  usb_host.SetListener(&runtime_data_plane);

  usb_host.Start({});
  ble_transport.Start({});
  supervisor.Start({});

#if CHARM_CFG_UART_RUNTIME_AVAILABLE
  StartConfigTransportRuntime(config_transport_runtime_adapter);
#endif
}

}  // namespace charm::app
