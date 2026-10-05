# `esp-radio` (0.17.0)

## What it does in this repo

Provides the Wi‑Fi/BLE controller stack for Espressif chips in the esp-rs ecosystem.

In `src/bin/main.rs` we:

- call `esp_radio::init()` to initialize the radio subsystem
- create a Wi‑Fi controller using `esp_radio::wifi::new(...)`

## Key links

- API docs (docs.rs): https://docs.rs/esp-radio/0.17.0/esp_radio/
- Crate page: https://crates.io/crates/esp-radio
- Upstream repo/issues: https://github.com/esp-rs/esp-hal (esp-rs workspace)

Note: docs.rs currently fails to build the crate documentation for `esp-radio` 0.17.0, so the authoritative hosted API docs are here (ESP32‑C3):
https://docs.espressif.com/projects/rust/esp-radio/0.17.0/esp32c3/esp_radio/index.html

## APIs you’ll likely use first

- `esp_radio::init() -> Result<_, _>`
- `esp_radio::wifi::new(&radio_init, peripherals.WIFI, config)`

## Gotchas

- `esp-radio` needs an async/time/scheduler integration. In this repo that is provided by `esp-rtos` (see `esp_rtos::start(...)`).
- Be mindful of large buffers (keep them off stack; this repo denies `clippy::large_stack_frames`).

## How to use it (ESP32‑C3)

The ESP32‑C3 docs list:

- `esp_radio::init` (function): initialize Wi‑Fi and/or BLE
- `esp_radio::wifi` module: Wi‑Fi API surface
- `esp_radio::Controller` (struct): controller for the radio driver

Reference (ESP32‑C3 docs): https://docs.espressif.com/projects/rust/esp-radio/0.17.0/esp32c3/esp_radio/index.html

### Minimal bring-up (what this repo already does)

```rust
use log::info;

// After esp-hal init + allocator + esp-rtos start...
let radio_init = esp_radio::init().expect("Failed to initialize Wi-Fi/BLE controller");
let (mut wifi_controller, _interfaces) =
    esp_radio::wifi::new(&radio_init, peripherals.WIFI, Default::default())
        .expect("Failed to initialize Wi-Fi controller");

info!("Wi-Fi controller created: {:?}", core::any::type_name::<_>());
let _ = wifi_controller;
```

### Important build note (from esp-radio docs)

The ESP32‑C3 docs warn that Wi‑Fi often requires higher optimization levels; for debug builds, it recommends:

```toml
[profile.dev.package.esp-radio]
opt-level = 3
```

Reference (Optimization Level section): https://docs.espressif.com/projects/rust/esp-radio/0.17.0/esp32c3/esp_radio/index.html

### Configuration via env / .cargo/config.toml

The ESP32‑C3 docs list tunables that can be set via environment variables (or Cargo `[env]` in `.cargo/config.toml`), e.g. MTU and queue sizes.

Reference (Additional configuration table): https://docs.espressif.com/projects/rust/esp-radio/0.17.0/esp32c3/esp_radio/index.html

## Complete API inventory

Full API index: See local docs at `target/riscv32imc-unknown-none-elf/doc/esp_radio/all.html`

### Top-level Structs
- **`Controller`**: Controller for the ESP radio driver (top-level handle).

### Top-level Enums
- **`InitializationError`**: Error returned by `init()`.

### Top-level Functions
- **`init()`**: Initialize radio subsystem for Wi‑Fi and/or BLE.
- **`phy_calibration_data()`**: Get PHY calibration blob.
- **`set_phy_calibration_data(...)`**: Set PHY calibration blob.
- **`wifi_set_log_verbose()`**: Enable verbose Wi‑Fi driver logging (feature-gated).

### Constants
- **`esp_now::BROADCAST_ADDRESS`**: ESP-NOW broadcast MAC address.
- **`esp_now::ESP_NOW_MAX_DATA_LEN`**: Maximum ESP-NOW data length.

### Modules

#### `esp_radio::wifi`
**Core Types:**
- **`WifiController`**: Main Wi‑Fi controller handle.
- **`WifiDevice`**: Wi‑Fi device abstraction.
- **`Interfaces`**: Wi‑Fi interface handles (station/AP).
- **`Controller`**: Radio controller (top-level handle).
- **`wifi::Config`**: Wi‑Fi configuration.
- **`wifi::ClientConfig`**: Station (client) configuration.
- **`wifi::AccessPointConfig`**: Access point configuration.
- **`wifi::ScanConfig`**: Wi‑Fi scan configuration.
- **`wifi::Country`**: Country code configuration.
- **`wifi::CountryInfo`**: Country information.
- **`wifi::AccessPointInfo`**: Access point information.
- **`wifi::Interfaces`**: Wi‑Fi interfaces handle.
- **`wifi::WifiController`**: Wi‑Fi controller handle.
- **`wifi::WifiDevice`**: Wi‑Fi device handle.
- **`wifi::event::ActionTxStatus`**: Action TX status event.
- **`wifi::event::ApCredential`**: AP credential event.
- **`wifi::event::ApProbeReqReceived`**: AP probe request received event.
- **`wifi::event::ApStaConnected`**: AP station connected event.
- **`wifi::event::ApStaDisconnected`**: AP station disconnected event.
- **`wifi::event::ApStart`**: AP started event.
- **`wifi::event::ApStop`**: AP stopped event.
- **`wifi::event::ApWpsRgFailed`**: AP WPS registrar failed event.
- **`wifi::event::ApWpsRgPbcOverlap`**: AP WPS registrar PBC overlap event.
- **`wifi::event::ApWpsRgPin`**: AP WPS registrar PIN event.
- **`wifi::event::ApWpsRgSuccess`**: AP WPS registrar success event.
- **`wifi::event::ApWpsRgTimeout`**: AP WPS registrar timeout event.
- **`wifi::event::BtwtSetup`**: BTWT setup event.
- **`wifi::event::BtwtTeardown`**: BTWT teardown event.
- **`wifi::event::ConnectionlessModuleWakeIntervalStart`**: Connectionless module wake interval start event.
- **`wifi::event::FtmReport`**: FTM report event.
- **`wifi::event::FtmReportEntry`**: FTM report entry.
- **`wifi::event::HomeChannelChange`**: Home channel changed event.
- **`wifi::event::ItwtProbe`**: iTWT probe event.
- **`wifi::event::ItwtSetup`**: iTWT setup event.
- **`wifi::event::ItwtSuspend`**: iTWT suspend event.
- **`wifi::event::ItwtTeardown`**: iTWT teardown event.
- **`wifi::event::NanReceive`**: NAN receive event.
- **`wifi::event::NanReplied`**: NAN replied event.
- **`wifi::event::NanStarted`**: NAN started event.
- **`wifi::event::NanStopped`**: NAN stopped event.
- **`wifi::event::NanSvcMatch`**: NAN service match event.
- **`wifi::event::NdpConfirm`**: NDP confirm event.
- **`wifi::event::NdpIndication`**: NDP indication event.
- **`wifi::event::NdpTerminated`**: NDP terminated event.
- **`wifi::event::RocDone`**: ROC done event.
- **`wifi::event::ScanDone`**: Scan done event.
- **`wifi::event::StaAuthmodeChange`**: Station auth mode changed event.
- **`wifi::event::StaBeaconTimeout`**: Station beacon timeout event.
- **`wifi::event::StaBssRssiLow`**: Station BSS RSSI low event.
- **`wifi::event::StaConnected`**: Station connected event.
- **`wifi::event::StaDisconnected`**: Station disconnected event.
- **`wifi::event::StaNeighborRep`**: Station neighbor report event.
- **`wifi::event::StaStart`**: Station started event.
- **`wifi::event::StaStop`**: Station stopped event.
- **`wifi::event::StaWpsErFailed`**: Station WPS ER failed event.
- **`wifi::event::StaWpsErPbcOverlap`**: Station WPS ER PBC overlap event.
- **`wifi::event::StaWpsErPin`**: Station WPS ER PIN event.
- **`wifi::event::StaWpsErSuccess`**: Station WPS ER success event.
- **`wifi::event::StaWpsErTimeout`**: Station WPS ER timeout event.
- **`wifi::event::TwtWakeup`**: TWT wakeup event.
- **`wifi::event::WifiReady`**: Wi‑Fi ready event.

**Enums:**
- **`InitializationError`**: Error returned by `init()`.
- **`wifi::AuthMethod`**: Authentication method.
- **`wifi::Capability`**: Wi‑Fi capability flags.
- **`wifi::InternalWifiError`**: Internal Wi‑Fi error.
- **`wifi::ModeConfig`**: Mode configuration.
- **`wifi::OperatingClass`**: Operating class.
- **`wifi::PowerSaveMode`**: Power save mode.
- **`wifi::Protocol`**: Wi‑Fi protocol (802.11b/g/n).
- **`wifi::ScanMethod`**: Scan method.
- **`wifi::ScanTypeConfig`**: Scan type configuration.
- **`wifi::SecondaryChannel`**: Secondary channel configuration.
- **`wifi::WifiApState`**: Access point state.
- **`wifi::WifiError`**: Wi‑Fi error.
- **`wifi::WifiEvent`**: Wi‑Fi event type.
- **`wifi::WifiMode`**: Wi‑Fi mode (Station, AccessPoint, StationAp).
- **`wifi::WifiStaState`**: Station state.

**Functions:**
- **`init()`**: Initialize radio subsystem for Wi‑Fi and/or BLE.
- **`phy_calibration_data()`**: Get PHY calibration data.
- **`set_phy_calibration_data(data)`**: Set PHY calibration data.
- **`wifi_set_log_verbose(enable)`**: Enable verbose Wi‑Fi driver logging (feature-gated).
- **`wifi::new(radio_init, wifi_peripheral, config)`**: Create a new Wi‑Fi controller.
- **`wifi::sta_mac()`**: Get station MAC address.
- **`wifi::ap_mac()`**: Get access point MAC address.
- **`wifi::sta_state()`**: Get station state.
- **`wifi::ap_state()`**: Get access point state.
- **`wifi::event::handle(event)`**: Handle a Wi‑Fi event.

**Event Types (`wifi::event`):**
- **`WifiReady`**: Wi‑Fi ready event.
- **`StaStart`**: Station started.
- **`StaStop`**: Station stopped.
- **`StaConnected`**: Station connected to AP.
- **`StaDisconnected`**: Station disconnected.
- **`StaAuthmodeChange`**: Station auth mode changed.
- **`StaBeaconTimeout`**: Station beacon timeout.
- **`StaBssRssiLow`**: Station BSS RSSI low.
- **`StaWpsErSuccess`**: Station WPS ER success.
- **`StaWpsErFailed`**: Station WPS ER failed.
- **`StaWpsErTimeout`**: Station WPS ER timeout.
- **`StaWpsErPin`**: Station WPS ER PIN.
- **`StaWpsErPbcOverlap`**: Station WPS ER PBC overlap.
- **`StaNeighborRep`**: Station neighbor report.
- **`ApStart`**: Access point started.
- **`ApStop`**: Access point stopped.
- **`ApStaConnected`**: AP station connected.
- **`ApStaDisconnected`**: AP station disconnected.
- **`ApProbeReqReceived`**: AP probe request received.
- **`ApCredential`**: AP credential.
- **`ActionTxStatus`**: Action TX status.
- **`RocDone`**: ROC (remain on channel) done.
- **`ScanDone`**: Scan done.
- **`HomeChannelChange`**: Home channel changed.
- **`FtmReport`**: FTM (fine timing measurement) report.
- **`FtmReportEntry`**: FTM report entry.
- **`TwtWakeup`**: TWT (target wake time) wakeup.
- **`BtwtSetup`**: BTWT setup.
- **`BtwtTeardown`**: BTWT teardown.
- **`ItwtSetup`**: iTWT setup.
- **`ItwtTeardown`**: iTWT teardown.
- **`ItwtProbe`**: iTWT probe.
- **`ItwtSuspend`**: iTWT suspend.
- **`ConnectionlessModuleWakeIntervalStart`**: Connectionless module wake interval start.
- **`NanStarted`**: NAN started.
- **`NanStopped`**: NAN stopped.
- **`NanReceive`**: NAN receive.
- **`NanReplied`**: NAN replied.
- **`NanSvcMatch`**: NAN service match.
- **`NdpConfirm`**: NDP (neighbor discovery protocol) confirm.
- **`NdpIndication`**: NDP indication.
- **`NdpTerminated`**: NDP terminated.
- **`ApWpsRgSuccess`**: AP WPS registrar success.
- **`ApWpsRgFailed`**: AP WPS registrar failed.
- **`ApWpsRgTimeout`**: AP WPS registrar timeout.
- **`ApWpsRgPin`**: AP WPS registrar PIN.
- **`ApWpsRgPbcOverlap`**: AP WPS registrar PBC overlap.

**Event Traits:**
- **`wifi::event::EventExt`**: Extension trait for Wi‑Fi events.

**Type Aliases:**
- **`wifi::event::Handler`**: Event handler function type.

#### `esp_radio::esp_now` (feature-gated)
- **`EspNow`**: ESP-NOW manager.
- **`EspNowManager`**: ESP-NOW manager handle.
- **`EspNowReceiver`**: ESP-NOW receiver.
- **`EspNowSender`**: ESP-NOW sender.
- **`PeerInfo`**: ESP-NOW peer information.
- **`PeerCount`**: ESP-NOW peer count.
- **`ReceiveFuture`**: Future for receiving ESP-NOW data.
- **`ReceiveInfo`**: ESP-NOW receive information.
- **`ReceivedData`**: ESP-NOW received data.
- **`SendFuture`**: Future for sending ESP-NOW data.
- **`SendWaiter`**: ESP-NOW send waiter.
- **`Error`**: ESP-NOW error.
- **`EspNowError`**: ESP-NOW error type.
- **`EspNowWifiInterface`**: ESP-NOW Wi‑Fi interface.
- **`WifiPhyRate`**: Wi‑Fi PHY rate.

#### `esp_radio::ble` (feature-gated / unstable)
- **`Config`**: BLE configuration.
- **`InvalidConfigError`**: Invalid BLE configuration error.
- **`ReceivedPacket`**: BLE received packet.
- **`ble::controller::BleConnector`**: BLE connector.
- **`ble::controller::BleConnectorError`**: BLE connector error.
- **`have_hci_read_data()`**: Check if HCI data is available.
- **`read_hci(...)`**: Read HCI data.


