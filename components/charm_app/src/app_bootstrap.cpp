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
#include <cstring>

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
static charm::core::InMemoryDeviceRegistry device_registry;
static charm::core::DefaultHidDescriptorParser descriptor_parser;
static charm::core::DefaultDecodePlanBuilder decode_plan_builder;
static charm::core::DefaultHidDecoder hid_decoder;
static charm::platform::UsbHostAdapter usb_host;
static charm::platform::BleTransportAdapter ble_transport;
static charm::platform::ConfigStoreNvs config_store;
static ConfigTransportService config_transport_service(config_store);
static ConfigTransportRuntimeAdapter config_transport_runtime_adapter(config_transport_service);
static charm::core::DefaultSupervisor supervisor;
static charm::core::DefaultRecoveryPolicy recovery_policy(supervisor);
static RuntimeDataPlane runtime_data_plane(usb_host, ble_transport, device_registry,
                                           descriptor_parser, decode_plan_builder,
                                           hid_decoder, mapping_engine,
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

  while (config_runtime_running) {
    const int read = uart_read_bytes(kConfigUartPort, rx.data(), rx.size(),
                                     pdMS_TO_TICKS(25));
    if (read > 0) {
      adapter->ConsumeBytes(rx.data(), static_cast<std::size_t>(read));
    }
    while (adapter->HasPendingFrame()) {
      if (!adapter->WritePendingFrame(writer)) {
        break;
      }
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

  (void)uart_driver_install(kConfigUartPort, kConfigRxBufferBytes,
                            kConfigTxBufferBytes, 0, nullptr, 0);
  (void)uart_param_config(kConfigUartPort, &uart_cfg);

  config_runtime_running = true;
  (void)xTaskCreatePinnedToCore(&ConfigTransportRuntimeTask, "cfg_uart_runtime",
                                4096, &adapter, 5, &config_runtime_task,
                                tskNO_AFFINITY);
}
#endif

}  // namespace

void InitializeAndRun() {
  if (!InitializeStorageAndActivate(config_store, supervisor,
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
