#include "charm/contracts/common_types.hpp"
#include "charm/contracts/error_types.hpp"
#include "charm/contracts/events.hpp"
#include "charm/contracts/identity_types.hpp"
#include "charm/contracts/report_types.hpp"
#include "charm/contracts/requests.hpp"
#include "charm/contracts/status_types.hpp"
#include "charm/contracts/transport_types.hpp"
#include "charm/contracts/registry_types.hpp"
#include "charm/ports/ble_transport_port.hpp"
#include "charm/ports/config_store_port.hpp"
#include "charm/ports/time_port.hpp"
#include "charm/ports/usb_host_port.hpp"
#include "charm/core/control_plane.hpp"
#include "charm/core/decode_plan.hpp"
#include "charm/core/hid_decoder.hpp"
#include "charm/core/hid_semantic_model.hpp"
#include "charm/core/device_registry.hpp"
#include "charm/core/supervisor.hpp"
#include "charm/app/app_bootstrap.hpp"

extern "C" void app_main(void) {
  charm::app::InitializeAndRun();
}
