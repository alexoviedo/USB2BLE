#include "charm/platform/ble_transport_adapter.hpp"

#include <algorithm>
#include <cstring>

#if __has_include("esp_bt.h") && __has_include("esp_bt_main.h") && \
    __has_include("esp_gap_ble_api.h") && __has_include("esp_gatts_api.h")
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#define CHARM_BLE_ESP_STACK_AVAILABLE 1
#else
#define CHARM_BLE_ESP_STACK_AVAILABLE 0
#endif

namespace charm::platform {

namespace {

constexpr std::uint16_t kMinReportBytes = 1;

#if CHARM_BLE_ESP_STACK_AVAILABLE
constexpr const char* kTag = "charm_ble_hid";
constexpr std::uint16_t kAppearanceGamepad = 0x03C0;
constexpr std::uint16_t kHidServiceUuid = 0x1812;
constexpr std::uint16_t kHidReportCharUuid = 0x2A4D;
constexpr std::uint16_t kClientConfigUuid = 0x2902;
constexpr std::uint16_t kHidInfoUuid = 0x2A4A;
constexpr std::uint16_t kProtocolModeUuid = 0x2A4E;
constexpr std::uint16_t kControlPointUuid = 0x2A4C;
constexpr std::uint16_t kReportMapUuid = 0x2A4B;
constexpr std::uint8_t kBleAppId = 0x42;
constexpr const char* kDeviceName = "Charm Gamepad";

constexpr std::uint8_t kHidReportMap[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x10,        //   Usage Maximum (16)
    0x15, 0x00,        //   Logical Min (0)
    0x25, 0x01,        //   Logical Max (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x10,        //   Report Count (16)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x00,        //   Logical Min (0)
    0x25, 0x07,        //   Logical Max (7)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x75, 0x04,        //   Report Size (4) padding
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x03,        //   Input (Const,Var,Abs)
    0x09, 0x30,        //   Usage X
    0x09, 0x31,        //   Usage Y
    0x09, 0x33,        //   Usage Rx
    0x09, 0x34,        //   Usage Ry
    0x15, 0x80,        //   Logical Min (-128)
    0x25, 0x7F,        //   Logical Max (127)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x05, 0x02,        //   Usage Page (Simulation Controls)
    0x09, 0xC5,        //   Usage (Brake)
    0x09, 0xC4,        //   Usage (Accelerator)
    0x15, 0x00,        //   Logical Min (0)
    0x26, 0xFF, 0x00,  //   Logical Max (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0xC0,              // End Collection
};

constexpr std::uint8_t kHidInfo[] = {0x11, 0x01, 0x00, 0x02};  // bcdHID, country, flags
constexpr std::uint8_t kProtocolModeReport = 0x01;
constexpr std::uint8_t kControlPointSuspend = 0x00;

#endif

}  // namespace

class EspBleLifecycleBackend final : public BleLifecycleBackend {
 public:
  bool RegisterStackEventSink(StackEventSink* sink) override {
    sink_ = sink;
#if CHARM_BLE_ESP_STACK_AVAILABLE
    if (sink_ == nullptr) {
      return false;
    }
    active_instance_ = this;
    if (esp_ble_gap_register_callback(&EspBleLifecycleBackend::GapCallback) != ESP_OK) {
      active_instance_ = nullptr;
      return false;
    }
    if (esp_ble_gatts_register_callback(&EspBleLifecycleBackend::GattsCallback) != ESP_OK) {
      active_instance_ = nullptr;
      return false;
    }
    ESP_LOGI(kTag, "BLE GAP/GATTS callbacks registered");
#endif
    return true;
  }

  bool UsesStackEventCallbacks() const override {
    return CHARM_BLE_ESP_STACK_AVAILABLE == 1;
  }

  bool Start() override {
#if CHARM_BLE_ESP_STACK_AVAILABLE
    if (started_) {
      return true;
    }
    ESP_LOGI(kTag, "Starting BLE HID backend");
    if (!InitControllerAndBluedroid()) {
      ESP_LOGE(kTag, "controller/bluedroid init failed");
      return false;
    }
    ConfigureSecurity();
    if (esp_ble_gatts_app_register(kBleAppId) != ESP_OK) {
      ESP_LOGE(kTag, "esp_ble_gatts_app_register failed");
      return false;
    }
    started_ = true;
#endif
    return true;
  }

  bool Stop() override {
#if CHARM_BLE_ESP_STACK_AVAILABLE
    if (!started_) {
      return true;
    }
    ESP_LOGI(kTag, "Stopping BLE HID backend");
    if (advertising_) {
      (void)esp_ble_gap_stop_advertising();
      advertising_ = false;
    }
    if (service_handle_ != 0) {
      (void)esp_ble_gatts_stop_service(service_handle_);
    }
    if (gatts_if_ != ESP_GATT_IF_NONE) {
      (void)esp_ble_gatts_app_unregister(gatts_if_);
    }
    service_handle_ = 0;
    report_value_handle_ = 0;
    cccd_handle_ = 0;
    connection_id_ = 0;
    report_ready_ = false;
    connected_ = false;

    if (esp_bluedroid_disable() != ESP_OK) return false;
    if (esp_bluedroid_deinit() != ESP_OK) return false;
    if (esp_bt_controller_disable() != ESP_OK) return false;
    if (esp_bt_controller_deinit() != ESP_OK) return false;
    started_ = false;
#endif
    return true;
  }

  bool ConfigureReportChannel(std::uint32_t transport_if,
                              std::uint16_t connection_id,
                              std::uint16_t value_handle,
                              bool require_confirmation) override {
    transport_if_ = transport_if;
    connection_id_ = connection_id;
    value_handle_ = value_handle;
    require_confirmation_ = require_confirmation;
    report_ready_ = (value_handle != 0);
#if CHARM_BLE_ESP_STACK_AVAILABLE
    ESP_LOGI(kTag, "report channel configured conn=%u handle=%u confirm=%d",
             connection_id_, value_handle_, static_cast<int>(require_confirmation_));
#endif
    return report_ready_;
  }

  void ClearReportChannel() override {
    report_ready_ = false;
    value_handle_ = 0;
  }

  bool SendReport(const charm::contracts::EncodedInputReport& report) override {
    if (!report_ready_ || !connected_) return false;
    if (report.bytes == nullptr || report.size < kMinReportBytes) return false;
#if CHARM_BLE_ESP_STACK_AVAILABLE
    const auto gatts_if = static_cast<esp_gatt_if_t>(transport_if_);
    const auto ret = esp_ble_gatts_send_indicate(
        gatts_if, connection_id_, value_handle_, static_cast<uint16_t>(report.size),
        const_cast<std::uint8_t*>(report.bytes), require_confirmation_);
    if (ret != ESP_OK) {
      ESP_LOGE(kTag, "notify failed conn=%u handle=%u err=%d", connection_id_,
               value_handle_, static_cast<int>(ret));
      return false;
    }
    ESP_LOGD(kTag, "notify sent conn=%u bytes=%u report_id=%u", connection_id_,
             static_cast<unsigned>(report.size), report.report_id);
#else
    (void)report;
#endif
    return true;
  }

 private:
#if CHARM_BLE_ESP_STACK_AVAILABLE
  bool InitControllerAndBluedroid() {
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&cfg) != ESP_OK) return false;
    if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) return false;
    if (esp_bluedroid_init() != ESP_OK) return false;
    if (esp_bluedroid_enable() != ESP_OK) return false;
    return true;
  }

  void ConfigureSecurity() {
    const esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
    const esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    std::uint8_t key_size = 16;
    std::uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    std::uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    (void)esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req,
                                         sizeof(auth_req));
    (void)esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap,
                                         sizeof(iocap));
    (void)esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size,
                                         sizeof(key_size));
    (void)esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key,
                                         sizeof(init_key));
    (void)esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key,
                                         sizeof(rsp_key));
  }

  void ConfigureAdvertising() {
    static std::uint16_t hid_service_uuid = kHidServiceUuid;
    esp_ble_adv_data_t adv_data;
    std::memset(&adv_data, 0, sizeof(adv_data));
    adv_data.set_scan_rsp = false;
    adv_data.include_name = true;
    adv_data.include_txpower = true;
    adv_data.appearance = kAppearanceGamepad;
    adv_data.service_uuid_len = sizeof(hid_service_uuid);
    adv_data.p_service_uuid = reinterpret_cast<std::uint8_t*>(&hid_service_uuid);
    adv_data.flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;
    (void)esp_ble_gap_set_device_name(kDeviceName);
    (void)esp_ble_gap_config_adv_data(&adv_data);
  }

  void StartAdvertising() {
    esp_ble_adv_params_t adv_params;
    std::memset(&adv_params, 0, sizeof(adv_params));
    adv_params.adv_int_min = ESP_BLE_GAP_ADV_ITVL_MS(30);
    adv_params.adv_int_max = ESP_BLE_GAP_ADV_ITVL_MS(60);
    adv_params.adv_type = ADV_TYPE_IND;
    adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    adv_params.channel_map = ADV_CHNL_ALL;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
    (void)esp_ble_gap_start_advertising(&adv_params);
  }

  void EnsureHidServiceCreated(esp_gatt_if_t gatts_if) {
    esp_gatt_srvc_id_t service_id;
    std::memset(&service_id, 0, sizeof(service_id));
    service_id.is_primary = true;
    service_id.id.inst_id = 0;
    service_id.id.uuid.len = ESP_UUID_LEN_16;
    service_id.id.uuid.uuid.uuid16 = kHidServiceUuid;
    (void)esp_ble_gatts_create_service(gatts_if, &service_id, 16);
  }

  void AddHidCharacteristics() {
    esp_bt_uuid_t uuid;
    std::memset(&uuid, 0, sizeof(uuid));
    esp_attr_value_t value;
    std::memset(&value, 0, sizeof(value));

    uuid.len = ESP_UUID_LEN_16;
    uuid.uuid.uuid16 = kHidInfoUuid;
    value.attr_max_len = sizeof(kHidInfo);
    value.attr_len = sizeof(kHidInfo);
    value.attr_value = const_cast<std::uint8_t*>(kHidInfo);
    (void)esp_ble_gatts_add_char(service_handle_, &uuid, ESP_GATT_PERM_READ,
                                 ESP_GATT_CHAR_PROP_BIT_READ, &value, nullptr);

    uuid.uuid.uuid16 = kReportMapUuid;
    value.attr_max_len = sizeof(kHidReportMap);
    value.attr_len = sizeof(kHidReportMap);
    value.attr_value = const_cast<std::uint8_t*>(kHidReportMap);
    (void)esp_ble_gatts_add_char(service_handle_, &uuid, ESP_GATT_PERM_READ,
                                 ESP_GATT_CHAR_PROP_BIT_READ, &value, nullptr);

    uuid.uuid.uuid16 = kProtocolModeUuid;
    value.attr_max_len = 1;
    value.attr_len = 1;
    value.attr_value = const_cast<std::uint8_t*>(&kProtocolModeReport);
    (void)esp_ble_gatts_add_char(service_handle_, &uuid,
                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                 ESP_GATT_CHAR_PROP_BIT_READ |
                                     ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                                 &value, nullptr);

    uuid.uuid.uuid16 = kControlPointUuid;
    value.attr_max_len = 1;
    value.attr_len = 1;
    value.attr_value = const_cast<std::uint8_t*>(&kControlPointSuspend);
    (void)esp_ble_gatts_add_char(service_handle_, &uuid, ESP_GATT_PERM_WRITE,
                                 ESP_GATT_CHAR_PROP_BIT_WRITE_NR, &value, nullptr);

    uuid.uuid.uuid16 = kHidReportCharUuid;
    std::uint8_t empty_report[9] = {0};
    value.attr_max_len = sizeof(empty_report);
    value.attr_len = sizeof(empty_report);
    value.attr_value = empty_report;
    (void)esp_ble_gatts_add_char(
        service_handle_, &uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY, &value, nullptr);
  }

  void UpdateBondingSnapshot() {
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) {
      bond_blob_.clear();
      return;
    }
    std::vector<esp_ble_bond_dev_t> list(dev_num);
    if (esp_ble_get_bond_device_list(&dev_num, list.data()) != ESP_OK || dev_num <= 0) {
      return;
    }
    bond_blob_.resize(static_cast<std::size_t>(dev_num) * ESP_BD_ADDR_LEN);
    for (int i = 0; i < dev_num; ++i) {
      std::memcpy(bond_blob_.data() + (i * ESP_BD_ADDR_LEN), list[i].bd_addr,
                  ESP_BD_ADDR_LEN);
    }
  }

  static void GapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    if (active_instance_ == nullptr || active_instance_->sink_ == nullptr) {
      return;
    }
    switch (event) {
      case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        active_instance_->StartAdvertising();
        break;
      case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param != nullptr &&
            param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
          active_instance_->advertising_ = true;
          ESP_LOGI(kTag, "advertising started");
          active_instance_->sink_->OnStackAdvertisingReady();
        } else {
          const std::uint32_t reason =
              param == nullptr ? 20u
                              : static_cast<std::uint32_t>(param->adv_start_cmpl.status);
          active_instance_->sink_->OnStackLifecycleError(reason);
        }
        break;
      case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (param != nullptr && param->ble_security.auth_cmpl.success) {
          active_instance_->UpdateBondingSnapshot();
          ESP_LOGI(kTag, "bonding established with peer");
        } else {
          const std::uint32_t reason =
              param == nullptr ? 21u
                              : static_cast<std::uint32_t>(
                                    param->ble_security.auth_cmpl.fail_reason);
          active_instance_->sink_->OnStackLifecycleError(reason);
        }
        break;
      case ESP_GAP_BLE_SEC_REQ_EVT:
        if (param != nullptr) {
          (void)esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        }
        break;
      default:
        break;
    }
  }

  static void GattsCallback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                            esp_ble_gatts_cb_param_t* param) {
    if (active_instance_ == nullptr || active_instance_->sink_ == nullptr) {
      return;
    }
    switch (event) {
      case ESP_GATTS_REG_EVT:
        active_instance_->gatts_if_ = gatts_if;
        active_instance_->ConfigureAdvertising();
        active_instance_->EnsureHidServiceCreated(gatts_if);
        ESP_LOGI(kTag, "GATTS app registered");
        break;
      case ESP_GATTS_CREATE_EVT:
        if (param == nullptr) break;
        active_instance_->service_handle_ = param->create.service_handle;
        (void)esp_ble_gatts_start_service(active_instance_->service_handle_);
        active_instance_->AddHidCharacteristics();
        ESP_LOGI(kTag, "HID service created handle=%u",
                 active_instance_->service_handle_);
        break;
      case ESP_GATTS_ADD_CHAR_EVT:
        if (param == nullptr || param->add_char.status != ESP_GATT_OK) break;
        if (param->add_char.char_uuid.len == ESP_UUID_LEN_16 &&
            param->add_char.char_uuid.uuid.uuid16 == kHidReportCharUuid) {
          active_instance_->report_value_handle_ = param->add_char.attr_handle;
          esp_bt_uuid_t descr_uuid;
          std::memset(&descr_uuid, 0, sizeof(descr_uuid));
          descr_uuid.len = ESP_UUID_LEN_16;
          descr_uuid.uuid.uuid16 = kClientConfigUuid;
          (void)esp_ble_gatts_add_char_descr(
              active_instance_->service_handle_, &descr_uuid,
              ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED, nullptr, nullptr);
        }
        break;
      case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        if (param != nullptr) {
          active_instance_->cccd_handle_ = param->add_char_descr.attr_handle;
        }
        break;
      case ESP_GATTS_CONNECT_EVT: {
        if (param == nullptr) break;
        active_instance_->connected_ = true;
        active_instance_->connection_id_ = param->connect.conn_id;
        charm::ports::BlePeerInfo peer{};
        for (std::size_t i = 0; i < peer.address.size(); ++i) {
          peer.address[i] = param->connect.remote_bda[i];
        }
        ESP_LOGI(kTag, "peer connected conn_id=%u", param->connect.conn_id);
        active_instance_->sink_->OnStackPeerConnected(peer);
        break;
      }
      case ESP_GATTS_DISCONNECT_EVT: {
        if (param == nullptr) break;
        active_instance_->connected_ = false;
        active_instance_->report_ready_ = false;
        charm::ports::BlePeerInfo peer{};
        for (std::size_t i = 0; i < peer.address.size(); ++i) {
          peer.address[i] = param->disconnect.remote_bda[i];
        }
        ESP_LOGI(kTag, "peer disconnected reason=%u", param->disconnect.reason);
        active_instance_->sink_->OnStackReportChannelClosed();
        active_instance_->sink_->OnStackPeerDisconnected(peer);
        active_instance_->StartAdvertising();
        break;
      }
      case ESP_GATTS_WRITE_EVT: {
        if (param == nullptr || param->write.is_prep) break;
        if (param->write.handle == active_instance_->cccd_handle_ &&
            param->write.len >= 2) {
          const std::uint16_t cccd = static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(param->write.value[0]) |
              (static_cast<std::uint16_t>(param->write.value[1]) << 8u));
          if (cccd == 0x0000u) {
            ESP_LOGI(kTag, "report channel closed by CCCD");
            active_instance_->sink_->OnStackReportChannelClosed();
          } else if (cccd == 0x0001u || cccd == 0x0002u) {
            ESP_LOGI(kTag, "report channel ready cccd=0x%04x", cccd);
            active_instance_->sink_->OnStackReportChannelReady(
                static_cast<std::uint32_t>(gatts_if), param->write.conn_id,
                active_instance_->report_value_handle_, cccd == 0x0002u);
          }
        }
        break;
      }
      default:
        break;
    }
  }
#endif

  StackEventSink* sink_{nullptr};
  std::uint32_t transport_if_{0};
  std::uint16_t connection_id_{0};
  std::uint16_t value_handle_{0};
  bool require_confirmation_{false};
  bool report_ready_{false};
  bool connected_{false};
  std::vector<std::uint8_t> bond_blob_{};

#if CHARM_BLE_ESP_STACK_AVAILABLE
  bool started_{false};
  bool advertising_{false};
  esp_gatt_if_t gatts_if_{ESP_GATT_IF_NONE};
  std::uint16_t service_handle_{0};
  std::uint16_t report_value_handle_{0};
  std::uint16_t cccd_handle_{0};
  static inline EspBleLifecycleBackend* active_instance_{nullptr};
#endif
};

BleTransportAdapter::BleTransportAdapter()
    : backend_(std::make_unique<EspBleLifecycleBackend>()) {}

BleTransportAdapter::BleTransportAdapter(std::unique_ptr<BleLifecycleBackend> backend)
    : backend_(backend ? std::move(backend) : std::make_unique<EspBleLifecycleBackend>()) {}

charm::contracts::StartResult BleTransportAdapter::Start(
    const charm::contracts::StartRequest& /*request*/) {
  charm::contracts::StartResult result;
  if (state_ == charm::contracts::AdapterState::kRunning) {
    result.status = charm::contracts::ContractStatus::kRejected;
    return result;
  }

  if (backend_ == nullptr || !backend_->RegisterStackEventSink(this) ||
      !backend_->Start()) {
    result.status = charm::contracts::ContractStatus::kFailed;
    result.fault_code.category = charm::contracts::ErrorCategory::kAdapterFailure;
    result.fault_code.reason = 1;
    EmitStatus(charm::contracts::ContractStatus::kFailed,
               charm::contracts::AdapterState::kStopped,
               charm::contracts::ErrorCategory::kAdapterFailure, 1);
    return result;
  }

  state_ = charm::contracts::AdapterState::kRunning;
  advertising_ready_ = false;
  peer_connected_ = false;
  report_channel_ready_ = false;
  recovery_attempts_ = 0;
  result.status = charm::contracts::ContractStatus::kOk;
  if (!backend_->UsesStackEventCallbacks()) {
    OnAdvertisingReady();
  }
  return result;
}

charm::contracts::StopResult BleTransportAdapter::Stop(
    const charm::contracts::StopRequest& /*request*/) {
  charm::contracts::StopResult result;
  if (state_ == charm::contracts::AdapterState::kStopped) {
    result.status = charm::contracts::ContractStatus::kRejected;
    return result;
  }

  if (backend_ == nullptr || !backend_->Stop()) {
    result.status = charm::contracts::ContractStatus::kFailed;
    result.fault_code.category = charm::contracts::ErrorCategory::kAdapterFailure;
    result.fault_code.reason = 2;
    EmitStatus(charm::contracts::ContractStatus::kFailed, state_,
               charm::contracts::ErrorCategory::kAdapterFailure, 2);
    return result;
  }

  state_ = charm::contracts::AdapterState::kStopped;
  advertising_ready_ = false;
  peer_connected_ = false;
  report_channel_ready_ = false;
  recovery_attempts_ = 0;
  if (backend_ != nullptr) {
    backend_->ClearReportChannel();
  }
  result.status = charm::contracts::ContractStatus::kOk;
  EmitStatus(charm::contracts::ContractStatus::kOk, state_,
             charm::contracts::ErrorCategory::kAdapterFailure, 0);
  return result;
}

charm::ports::NotifyInputReportResult BleTransportAdapter::NotifyInputReport(
    const charm::ports::NotifyInputReportRequest& request) {
  charm::ports::NotifyInputReportResult result;

  if (state_ != charm::contracts::AdapterState::kRunning) {
    result.status = charm::contracts::ContractStatus::kRejected;
    return result;
  }

  if (!advertising_ready_) {
    result.status = charm::contracts::ContractStatus::kUnavailable;
    result.fault_code.category = charm::contracts::ErrorCategory::kInvalidState;
    result.fault_code.reason = 3;
    return result;
  }

  if (!peer_connected_ || !report_channel_ready_) {
    result.status = charm::contracts::ContractStatus::kUnavailable;
    result.fault_code.category = charm::contracts::ErrorCategory::kInvalidState;
    result.fault_code.reason = 4;
    return result;
  }

  if (backend_ == nullptr || !backend_->SendReport(request.report)) {
    result.status = charm::contracts::ContractStatus::kFailed;
    result.fault_code.category = charm::contracts::ErrorCategory::kTransportFailure;
    result.fault_code.reason = 5;
    const bool recovered = TryRecover(5);
    EmitStatus(charm::contracts::ContractStatus::kFailed, state_,
               charm::contracts::ErrorCategory::kTransportFailure,
               recovered ? 0u : 5u);
    return result;
  }

  result.status = charm::contracts::ContractStatus::kOk;
  return result;
}

void BleTransportAdapter::SetListener(charm::ports::BleTransportPortListener* listener) {
  listener_ = listener;
}

void BleTransportAdapter::OnAdvertisingReady() {
  if (state_ != charm::contracts::AdapterState::kRunning) return;
  advertising_ready_ = true;
  EmitStatus(charm::contracts::ContractStatus::kOk, state_,
             charm::contracts::ErrorCategory::kAdapterFailure, 0);
}

void BleTransportAdapter::OnPeerConnected(const charm::ports::BlePeerInfo& peer_info) {
  if (state_ != charm::contracts::AdapterState::kRunning || listener_ == nullptr) return;
  peer_connected_ = true;
  active_peer_ = peer_info;
  listener_->OnPeerConnected(peer_info);
}

void BleTransportAdapter::OnPeerDisconnected(const charm::ports::BlePeerInfo& peer_info) {
  if (listener_ == nullptr) return;
  peer_connected_ = false;
  active_peer_ = peer_info;
  OnReportChannelClosed();
  listener_->OnPeerDisconnected(peer_info);
}

void BleTransportAdapter::OnLifecycleError(std::uint32_t reason) {
  advertising_ready_ = false;
  (void)TryRecover(reason);
  EmitStatus(charm::contracts::ContractStatus::kFailed, state_,
             charm::contracts::ErrorCategory::kAdapterFailure, reason);
}

void BleTransportAdapter::OnReportChannelReady(std::uint32_t transport_if,
                                               std::uint16_t connection_id,
                                               std::uint16_t value_handle,
                                               bool require_confirmation) {
  if (state_ != charm::contracts::AdapterState::kRunning || backend_ == nullptr) return;
  if (!peer_connected_) {
    report_channel_ready_ = false;
    return;
  }
  report_channel_ready_ = backend_->ConfigureReportChannel(
      transport_if, connection_id, value_handle, require_confirmation);
  if (!report_channel_ready_) {
    (void)TryRecover(6);
  }
}

void BleTransportAdapter::OnReportChannelClosed() {
  report_channel_ready_ = false;
  if (backend_ != nullptr) {
    backend_->ClearReportChannel();
  }
}

void BleTransportAdapter::SetBondingMaterial(const std::uint8_t* bytes, std::size_t size) {
  if (bytes == nullptr || size == 0) {
    bonding_material_.clear();
    return;
  }
  bonding_material_.assign(bytes, bytes + size);
}

charm::ports::BondingMaterialRef BleTransportAdapter::GetBondingMaterial() const {
  charm::ports::BondingMaterialRef ref{};
  ref.bytes = bonding_material_.empty() ? nullptr : bonding_material_.data();
  ref.size = bonding_material_.size();
  return ref;
}

void BleTransportAdapter::ClearBondingMaterial() {
  bonding_material_.clear();
}

void BleTransportAdapter::OnStackAdvertisingReady() { OnAdvertisingReady(); }

void BleTransportAdapter::OnStackPeerConnected(
    const charm::ports::BlePeerInfo& peer_info) {
  OnPeerConnected(peer_info);
  bonding_material_.assign(peer_info.address.begin(), peer_info.address.end());
}

void BleTransportAdapter::OnStackPeerDisconnected(
    const charm::ports::BlePeerInfo& peer_info) {
  OnPeerDisconnected(peer_info);
}

void BleTransportAdapter::OnStackLifecycleError(std::uint32_t reason) {
  OnLifecycleError(reason);
}

void BleTransportAdapter::OnStackReportChannelReady(std::uint32_t transport_if,
                                                    std::uint16_t connection_id,
                                                    std::uint16_t value_handle,
                                                    bool require_confirmation) {
  OnReportChannelReady(transport_if, connection_id, value_handle,
                       require_confirmation);
}

void BleTransportAdapter::OnStackReportChannelClosed() { OnReportChannelClosed(); }

bool BleTransportAdapter::TryRecover(std::uint32_t reason) {
  if (state_ != charm::contracts::AdapterState::kRunning || backend_ == nullptr) {
    return false;
  }

  if (recovery_attempts_ >= kMaxRecoveryAttempts) {
    state_ = charm::contracts::AdapterState::kStopped;
    advertising_ready_ = false;
    peer_connected_ = false;
    report_channel_ready_ = false;
    backend_->ClearReportChannel();
    (void)backend_->Stop();
    return false;
  }

  ++recovery_attempts_;
  report_channel_ready_ = false;
  peer_connected_ = false;
  backend_->ClearReportChannel();

  const bool stopped = backend_->Stop();
  const bool started = stopped && backend_->Start();
  if (!started) {
    state_ = charm::contracts::AdapterState::kStopped;
    advertising_ready_ = false;
    report_channel_ready_ = false;
    peer_connected_ = false;
    return false;
  }

  state_ = charm::contracts::AdapterState::kRunning;
  advertising_ready_ = false;
  OnAdvertisingReady();
  EmitStatus(charm::contracts::ContractStatus::kOk, state_,
             charm::contracts::ErrorCategory::kAdapterFailure, reason);
  return true;
}

void BleTransportAdapter::EmitStatus(charm::contracts::ContractStatus status,
                                     charm::contracts::AdapterState state,
                                     charm::contracts::ErrorCategory category,
                                     std::uint32_t reason) {
  if (listener_ == nullptr) return;
  charm::ports::BleTransportStatus status_event{};
  status_event.status = status;
  status_event.state = state;
  status_event.fault_code.category = category;
  status_event.fault_code.reason = reason;
  listener_->OnStatusChanged(status_event);
}

}  // namespace charm::platform
