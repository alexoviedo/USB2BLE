#include "charm/platform/usb_host_adapter.hpp"

#include <array>
#include <cstring>

#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

namespace charm::platform {

namespace {

#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
constexpr const char* kTag = "charm_usb_host";
constexpr std::size_t kMaxInputReportBytes = 128;
#endif

constexpr std::uint32_t kFaultStartInstallFailed = 100;
constexpr std::uint32_t kFaultStartDriverInstallFailed = 101;
constexpr std::uint32_t kFaultHostEventLoopFault = 102;
constexpr std::uint32_t kFaultOpenDeviceFailed = 103;
constexpr std::uint32_t kFaultOpenInterfaceFailed = 104;
constexpr std::uint32_t kFaultInputReportReadFailed = 105;
constexpr std::uint32_t kFaultTransferError = 106;

}  // namespace

UsbHostAdapter::UsbHostAdapter() = default;

UsbHostAdapter::~UsbHostAdapter() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    listener_ = nullptr;
  }
  (void)Stop({});
}

charm::contracts::StartResult UsbHostAdapter::Start(
    const charm::contracts::StartRequest& /*request*/) {
  std::lock_guard<std::mutex> lock(mutex_);
  charm::contracts::StartResult result{};
  if (started_) {
    result.status = charm::contracts::ContractStatus::kRejected;
    return result;
  }

  if (!InstallHostStack()) {
    result.status = charm::contracts::ContractStatus::kFailed;
    result.fault_code.category = charm::contracts::ErrorCategory::kAdapterFailure;
    result.fault_code.reason = kFaultStartInstallFailed;
    EmitStatusLocked(charm::contracts::ContractStatus::kFailed,
                     charm::contracts::AdapterState::kFaulted,
                     charm::contracts::ErrorCategory::kAdapterFailure,
                     kFaultStartInstallFailed);
    return result;
  }

  started_ = true;
  result.status = charm::contracts::ContractStatus::kOk;
  EmitStatusLocked(charm::contracts::ContractStatus::kOk,
                   charm::contracts::AdapterState::kReady,
                   charm::contracts::ErrorCategory::kContractViolation,
                   0);
  return result;
}

charm::contracts::StopResult UsbHostAdapter::Stop(
    const charm::contracts::StopRequest& /*request*/) {
  std::lock_guard<std::mutex> lock(mutex_);
  charm::contracts::StopResult result{};
  if (!started_) {
    result.status = charm::contracts::ContractStatus::kRejected;
    return result;
  }

  started_ = false;
  ShutdownInterfacesLocked();
  UninstallHostStack();

  devices_by_address_.clear();
  device_handle_to_address_.clear();
  interfaces_by_handle_.clear();

  EmitStatusLocked(charm::contracts::ContractStatus::kOk,
                   charm::contracts::AdapterState::kStopped,
                   charm::contracts::ErrorCategory::kContractViolation,
                   0);
  result.status = charm::contracts::ContractStatus::kOk;
  return result;
}

charm::ports::ClaimInterfaceResult UsbHostAdapter::ClaimInterface(
    const charm::ports::ClaimInterfaceRequest& request) {
  std::lock_guard<std::mutex> lock(mutex_);
  charm::ports::ClaimInterfaceResult result{};
  if (!started_) {
    result.status = charm::contracts::ContractStatus::kRejected;
    return result;
  }

#if !CHARM_USB_HOST_ESP_IDF_AVAILABLE
  result.interface_handle = charm::contracts::InterfaceHandle{next_interface_handle_id_++};
  result.status = charm::contracts::ContractStatus::kOk;
  return result;
#endif

  result.interface_handle =
      ResolveInterfaceHandleLocked(request.device_handle, request.interface_number);
  if (result.interface_handle.value == 0) {
    result.status = charm::contracts::ContractStatus::kRejected;
    result.fault_code.category = charm::contracts::ErrorCategory::kInvalidRequest;
    result.fault_code.reason = kFaultOpenInterfaceFailed;
    return result;
  }

  result.status = charm::contracts::ContractStatus::kOk;
  return result;
}

void UsbHostAdapter::SetListener(charm::ports::UsbHostPortListener* listener) {
  std::lock_guard<std::mutex> lock(mutex_);
  listener_ = listener;
}

void UsbHostAdapter::SimulateDeviceConnected(
    const charm::ports::UsbEnumerationInfo& info,
    const charm::ports::DeviceDescriptorRef& desc) {
  charm::ports::UsbHostPortListener* listener = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) {
      listener = listener_;
    }
  }
  if (listener != nullptr) {
    listener->OnDeviceConnected(info, desc);
  }
}

void UsbHostAdapter::SimulateDeviceDisconnected(
    charm::contracts::DeviceHandle device_handle) {
  charm::ports::UsbHostPortListener* listener = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) {
      listener = listener_;
    }
  }
  if (listener != nullptr) {
    listener->OnDeviceDisconnected(device_handle);
  }
}

void UsbHostAdapter::SimulateInterfaceDescriptorAvailable(
    const charm::ports::InterfaceDescriptorRef& desc) {
  charm::ports::UsbHostPortListener* listener = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) {
      listener = listener_;
    }
  }
  if (listener != nullptr) {
    listener->OnInterfaceDescriptorAvailable(desc);
  }
}

void UsbHostAdapter::SimulateReportReceived(
    const charm::contracts::RawHidReportRef& report_ref) {
  charm::ports::UsbHostPortListener* listener = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) {
      listener = listener_;
    }
  }
  if (listener != nullptr) {
    listener->OnReportReceived(report_ref);
  }
}

void UsbHostAdapter::SimulateStatusChanged(const charm::ports::UsbHostStatus& status) {
  charm::ports::UsbHostPortListener* listener = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    listener = listener_;
  }
  if (listener != nullptr) {
    listener->OnStatusChanged(status);
  }
}

void UsbHostAdapter::EmitStatusLocked(charm::contracts::ContractStatus status,
                                      charm::contracts::AdapterState state,
                                      charm::contracts::ErrorCategory category,
                                      std::uint32_t reason) {
  if (listener_ == nullptr) {
    return;
  }
  charm::ports::UsbHostStatus update{};
  update.status = status;
  update.state = state;
  update.fault_code.category = category;
  update.fault_code.reason = reason;
  listener_->OnStatusChanged(update);
}

charm::contracts::InterfaceHandle UsbHostAdapter::ResolveInterfaceHandleLocked(
    const charm::contracts::DeviceHandle& device_handle,
    charm::contracts::InterfaceNumber interface_number) {
  const auto addr_it = device_handle_to_address_.find(device_handle.value);
  if (addr_it == device_handle_to_address_.end()) {
    return {};
  }
  const auto dev_it = devices_by_address_.find(addr_it->second);
  if (dev_it == devices_by_address_.end()) {
    return {};
  }
  const auto iface_it =
      dev_it->second.claimed_interfaces.find(interface_number);
  if (iface_it == dev_it->second.claimed_interfaces.end()) {
    return {};
  }
  return iface_it->second;
}

void UsbHostAdapter::ShutdownInterfacesLocked() {
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
  for (auto& entry : interfaces_by_handle_) {
    if (entry.second.hid_handle != nullptr) {
      (void)hid_host_device_stop(entry.second.hid_handle);
      (void)hid_host_device_close(entry.second.hid_handle);
    }
  }
#endif
}

bool UsbHostAdapter::InstallHostStack() {
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
  ESP_LOGI(kTag, "Installing USB host stack");
  usb_host_config_t host_config;
  std::memset(&host_config, 0, sizeof(host_config));
  host_config.skip_phy_setup = false;
  host_config.intr_flags = ESP_INTR_FLAG_LOWMED;

  if (usb_host_install(&host_config) != ESP_OK) {
    ESP_LOGE(kTag, "usb_host_install failed");
    return false;
  }

  hid_host_driver_config_t hid_config;
  std::memset(&hid_config, 0, sizeof(hid_config));
  hid_config.create_background_task = true;
  hid_config.task_priority = 5;
  hid_config.stack_size = 4096;
  hid_config.core_id = tskNO_AFFINITY;
  hid_config.callback = &UsbHostAdapter::OnHidDriverEvent;
  hid_config.callback_arg = this;
  if (hid_host_install(&hid_config) != ESP_OK) {
    ESP_LOGE(kTag, "hid_host_install failed");
    (void)usb_host_uninstall();
    return false;
  }

  usb_lib_thread_running_.store(true);
  usb_lib_thread_ = std::thread([this]() {
    while (usb_lib_thread_running_.load()) {
      uint32_t event_flags = 0;
      const esp_err_t err = usb_host_lib_handle_events(pdMS_TO_TICKS(250), &event_flags);
      if (err != ESP_OK) {
        std::lock_guard<std::mutex> lock(mutex_);
        EmitStatusLocked(charm::contracts::ContractStatus::kFailed,
                         charm::contracts::AdapterState::kFaulted,
                         charm::contracts::ErrorCategory::kTransportFailure,
                         kFaultHostEventLoopFault);
        ESP_LOGE(kTag, "usb_host_lib_handle_events failed (%d)", static_cast<int>(err));
      }
      if ((event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) != 0U) {
        break;
      }
    }
  });

  ESP_LOGI(kTag, "USB host stack ready");
  return true;
#else
  return true;
#endif
}

void UsbHostAdapter::UninstallHostStack() {
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
  usb_lib_thread_running_.store(false);
  if (usb_lib_thread_.joinable()) {
    usb_lib_thread_.join();
  }

  (void)hid_host_uninstall();
  (void)usb_host_device_free_all();
  (void)usb_host_uninstall();
#endif
}

void UsbHostAdapter::OnHidDriverEvent(
    hid_host_device_handle_t hid_device_handle,
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
    hid_host_driver_event_t event,
#else
    int event,
#endif
    void* arg) {
  auto* self = static_cast<UsbHostAdapter*>(arg);
  if (self == nullptr) {
    return;
  }
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
  if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
    self->HandleHidDriverConnected(hid_device_handle);
  }
#else
  (void)hid_device_handle;
  (void)event;
#endif
}

void UsbHostAdapter::OnHidInterfaceEvent(
    hid_host_device_handle_t hid_device_handle,
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
    hid_host_interface_event_t event,
#else
    int event,
#endif
    void* arg) {
  auto* self = static_cast<UsbHostAdapter*>(arg);
  if (self == nullptr) {
    return;
  }
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
  switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
      self->HandleHidInputReport(hid_device_handle);
      break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
      self->HandleHidTransferError(hid_device_handle);
      break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
      self->HandleHidDisconnected(hid_device_handle);
      break;
    default:
      break;
  }
#else
  (void)hid_device_handle;
  (void)event;
#endif
}

void UsbHostAdapter::HandleHidDriverConnected(
    hid_host_device_handle_t hid_device_handle) {
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
  hid_host_dev_params_t params;
  std::memset(&params, 0, sizeof(params));
  if (hid_host_device_get_params(hid_device_handle, &params) != ESP_OK) {
    return;
  }

  hid_host_dev_info_t info;
  std::memset(&info, 0, sizeof(info));
  (void)hid_host_get_device_info(hid_device_handle, &info);

  hid_host_device_config_t dev_config;
  std::memset(&dev_config, 0, sizeof(dev_config));
  dev_config.callback = &UsbHostAdapter::OnHidInterfaceEvent;
  dev_config.callback_arg = this;

  if (hid_host_device_open(hid_device_handle, &dev_config) != ESP_OK) {
    ESP_LOGE(kTag, "hid_host_device_open failed, addr=%u iface=%u", params.addr,
             params.iface_num);
    std::lock_guard<std::mutex> lock(mutex_);
    EmitStatusLocked(charm::contracts::ContractStatus::kFailed,
                     charm::contracts::AdapterState::kFaulted,
                     charm::contracts::ErrorCategory::kAdapterFailure,
                     kFaultOpenDeviceFailed);
    return;
  }
  if (hid_host_device_start(hid_device_handle) != ESP_OK) {
    (void)hid_host_device_close(hid_device_handle);
    ESP_LOGE(kTag, "hid_host_device_start failed, addr=%u iface=%u", params.addr,
             params.iface_num);
    return;
  }

  const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(hid_device_handle);

  std::lock_guard<std::mutex> lock(mutex_);
  if (!started_) {
    return;
  }

  DeviceContext* device_ctx = nullptr;
  auto dev_it = devices_by_address_.find(params.addr);
  if (dev_it == devices_by_address_.end()) {
    DeviceContext new_dev{};
    new_dev.device_address = params.addr;
    new_dev.device_handle = charm::contracts::DeviceHandle{next_device_handle_id_++};
    new_dev.vendor_id = charm::contracts::VendorId{info.VID};
    new_dev.product_id = charm::contracts::ProductId{info.PID};
    new_dev.hub_path.depth = 0;

    std::array<std::uint8_t, 18> synthesized_device_desc{};
    synthesized_device_desc[0] = static_cast<std::uint8_t>(synthesized_device_desc.size());
    synthesized_device_desc[1] = 0x01;
    synthesized_device_desc[8] = static_cast<std::uint8_t>(info.VID & 0xFF);
    synthesized_device_desc[9] = static_cast<std::uint8_t>((info.VID >> 8) & 0xFF);
    synthesized_device_desc[10] = static_cast<std::uint8_t>(info.PID & 0xFF);
    synthesized_device_desc[11] = static_cast<std::uint8_t>((info.PID >> 8) & 0xFF);
    new_dev.device_descriptor.assign(synthesized_device_desc.begin(),
                                     synthesized_device_desc.end());

    device_handle_to_address_[new_dev.device_handle.value] = params.addr;
    auto inserted = devices_by_address_.emplace(params.addr, std::move(new_dev));
    device_ctx = &inserted.first->second;

    if (listener_ != nullptr) {
      charm::ports::UsbEnumerationInfo enum_info{};
      enum_info.device_handle = device_ctx->device_handle;
      enum_info.vendor_id = device_ctx->vendor_id;
      enum_info.product_id = device_ctx->product_id;
      enum_info.hub_path = device_ctx->hub_path;

      charm::ports::DeviceDescriptorRef device_desc{};
      device_desc.device_handle = device_ctx->device_handle;
      device_desc.descriptor.bytes = device_ctx->device_descriptor.data();
      device_desc.descriptor.size = device_ctx->device_descriptor.size();

      ESP_LOGI(kTag,
               "USB HID device connected addr=%u vid=0x%04x pid=0x%04x hub_depth=%u",
               params.addr, info.VID, info.PID, device_ctx->hub_path.depth);
      listener_->OnDeviceConnected(enum_info, device_desc);
    }
  } else {
    device_ctx = &dev_it->second;
  }

  HidInterfaceContext iface_ctx{};
  iface_ctx.device_handle = device_ctx->device_handle;
  iface_ctx.interface_number = charm::contracts::InterfaceNumber{params.iface_num};
  iface_ctx.device_address = params.addr;
  iface_ctx.hid_handle = hid_device_handle;
  iface_ctx.interface_handle = charm::contracts::InterfaceHandle{next_interface_handle_id_++};
  device_ctx->claimed_interfaces[params.iface_num] = iface_ctx.interface_handle;

  size_t report_desc_len = 0;
  uint8_t* report_desc = hid_host_get_report_descriptor(hid_device_handle, &report_desc_len);
  if (report_desc != nullptr && report_desc_len > 0) {
    iface_ctx.report_descriptor.assign(report_desc, report_desc + report_desc_len);
  }

  interfaces_by_handle_[key] = std::move(iface_ctx);
  auto& created_iface = interfaces_by_handle_.at(key);

  if (listener_ != nullptr) {
    charm::ports::InterfaceDescriptorRef interface_desc{};
    interface_desc.device_handle = created_iface.device_handle;
    interface_desc.interface_handle = created_iface.interface_handle;
    interface_desc.interface_number = created_iface.interface_number;
    interface_desc.descriptor.bytes = created_iface.report_descriptor.data();
    interface_desc.descriptor.size = created_iface.report_descriptor.size();

    ESP_LOGI(kTag,
             "HID interface ready addr=%u iface=%u handle=%u report_desc=%u bytes",
             params.addr, params.iface_num, created_iface.interface_handle.value,
             static_cast<unsigned>(created_iface.report_descriptor.size()));
    listener_->OnInterfaceDescriptorAvailable(interface_desc);
  }
#else
  (void)hid_device_handle;
#endif
}

void UsbHostAdapter::HandleHidInputReport(
    hid_host_device_handle_t hid_device_handle) {
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
  std::array<std::uint8_t, kMaxInputReportBytes> report{};
  size_t length = 0;
  if (hid_host_device_get_raw_input_report_data(hid_device_handle, report.data(),
                                                report.size(), &length) != ESP_OK) {
    std::lock_guard<std::mutex> lock(mutex_);
    EmitStatusLocked(charm::contracts::ContractStatus::kFailed,
                     charm::contracts::AdapterState::kRunning,
                     charm::contracts::ErrorCategory::kTransportFailure,
                     kFaultInputReportReadFailed);
    return;
  }

  const auto key = reinterpret_cast<std::uintptr_t>(hid_device_handle);
  std::lock_guard<std::mutex> lock(mutex_);
  const auto iface_it = interfaces_by_handle_.find(key);
  if (!started_ || listener_ == nullptr || iface_it == interfaces_by_handle_.end()) {
    return;
  }

  charm::contracts::RawHidReportRef report_ref{};
  report_ref.device_handle = iface_it->second.device_handle;
  report_ref.interface_handle = iface_it->second.interface_handle;
  report_ref.byte_length = length;
  report_ref.report_meta.report_id = length > 0 ? report[0] : 0;
  report_ref.timestamp.ticks =
      static_cast<std::uint64_t>(esp_timer_get_time());
  report_ref.bytes = report.data();

  ESP_LOGD(kTag, "HID input addr=%u iface=%u size=%u report_id=%u",
           iface_it->second.device_address, iface_it->second.interface_number,
           static_cast<unsigned>(length), report_ref.report_meta.report_id);
  listener_->OnReportReceived(report_ref);
#else
  (void)hid_device_handle;
#endif
}

void UsbHostAdapter::HandleHidTransferError(
    hid_host_device_handle_t hid_device_handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!started_) {
    return;
  }
  const auto key = reinterpret_cast<std::uintptr_t>(hid_device_handle);
  const auto iface_it = interfaces_by_handle_.find(key);
#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
  if (iface_it != interfaces_by_handle_.end()) {
    ESP_LOGW(kTag, "HID transfer error addr=%u iface=%u",
             iface_it->second.device_address, iface_it->second.interface_number);
  }
#endif
  EmitStatusLocked(charm::contracts::ContractStatus::kFailed,
                   charm::contracts::AdapterState::kRunning,
                   charm::contracts::ErrorCategory::kTransportFailure,
                   kFaultTransferError);
}

void UsbHostAdapter::HandleHidDisconnected(
    hid_host_device_handle_t hid_device_handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto key = reinterpret_cast<std::uintptr_t>(hid_device_handle);
  const auto iface_it = interfaces_by_handle_.find(key);
  if (iface_it == interfaces_by_handle_.end()) {
    return;
  }

  const auto dev_addr = iface_it->second.device_address;
  const auto device_handle = iface_it->second.device_handle;
  const auto iface_number = iface_it->second.interface_number;

#if CHARM_USB_HOST_ESP_IDF_AVAILABLE
  (void)hid_host_device_stop(hid_device_handle);
  (void)hid_host_device_close(hid_device_handle);
  ESP_LOGI(kTag, "HID interface disconnected addr=%u iface=%u", dev_addr,
           iface_number);
#endif

  interfaces_by_handle_.erase(iface_it);
  auto dev_it = devices_by_address_.find(dev_addr);
  if (dev_it != devices_by_address_.end()) {
    dev_it->second.claimed_interfaces.erase(iface_number);
    if (dev_it->second.claimed_interfaces.empty()) {
      if (listener_ != nullptr) {
        listener_->OnDeviceDisconnected(device_handle);
      }
      device_handle_to_address_.erase(device_handle.value);
      devices_by_address_.erase(dev_it);
    }
  }
}

}  // namespace charm::platform
