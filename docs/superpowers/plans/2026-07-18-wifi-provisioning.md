# Wi-Fi Provisioning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a non-blocking, one-minute captive-portal Wi-Fi setup flow that starts only on first boot without credentials or a deliberate five-second dual-button gesture.

**Architecture:** A pure state/gesture contract defines every allowed portal transition, including the invariant that connection failures never open the portal. `WifiProvisioningManager` owns WiFiManager and ESP32 Wi-Fi persistence; `OtaManager` consumes network state without owning credentials, while `main.cpp` supplies button samples and OLED/HIL presentation.

**Tech Stack:** PlatformIO, Arduino ESP32, tzapu/WiFiManager 2.0.17, ArduinoOTA, Preferences/NVS, C++11 compile-time contracts, Python unittest/Tkinter.

## Global Constraints

- Existing thermostat, sensor, NVS setpoint, compressor/fan state machine, five-minute fan cooldown, OTA safe-stop, and HIL output lock behavior must remain unchanged.
- Portal triggers only when no saved or compatible credentials exist at boot, or after both temperature buttons are held for at least 5000 ms.
- Connection timeout, repeated failure, and runtime disconnect must never trigger the portal.
- Every portal session ends after 60000 ms; without saved credentials the same boot enters `UnconfiguredIdle`, while the next boot offers the portal again.
- Portal, retry, and gesture processing use `millis()` and never block the control loop.
- OTA updating rejects portal requests; no Wi-Fi, AP, or OTA password appears in HIL or Git.

---

### Task 1: Pure provisioning and gesture contracts

**Files:**
- Create: `src/wifi_provisioning_contract.cpp`
- Create: `include/wifi_provisioning_state.h`

**Interfaces:**
- Produces: `WifiProvisioningState {NoCredentials, UnconfiguredIdle, Connecting, Connected, RetryWait, Portal}`.
- Produces: `WifiProvisioningStatus`, `wifiInitialStatus(bool,uint32_t)`, `wifiPortalStatus(uint32_t)`, `wifiPortalTimedOut(...)`, `wifiRetryStatus(uint32_t)`, `wifiRetryDue(...)`.
- Produces: `ProvisioningGesture`, `initialProvisioningGesture()`, and `advanceProvisioningGesture(gesture,bothPressed,otaUpdating,now)` returning `{gesture, triggerPortal, consumeButtons}`.

- [x] **Step 1: Write the failing compile-time contract**

Create assertions proving: no credentials starts in `NoCredentials`; portal times out exactly at 60000 ms into `UnconfiguredIdle`; retry returns to `Connecting`, never `Portal`; a 4999 ms dual hold does not trigger; 5000 ms triggers once; release rearms; OTA updating prevents a trigger.

- [x] **Step 2: Verify RED**

Run `pio run -e esp32-c3-devkitm-1`. Expected: compilation fails because `wifi_provisioning_state.h` does not exist.

- [x] **Step 3: Implement the minimal pure header**

Use constants `WIFI_CONNECT_TIMEOUT_MS = 15000u`, `WIFI_RETRY_INTERVAL_MS = 30000u`, `WIFI_PORTAL_TIMEOUT_MS = 60000u`, and `WIFI_PROVISIONING_HOLD_MS = 5000u`. All deadline checks use `static_cast<uint32_t>(now - startedAt)`.

- [x] **Step 4: Verify GREEN and commit**

Run the release build and commit `test: define Wi-Fi provisioning contracts` after all static assertions compile.

### Task 2: Non-blocking Wi-Fi provisioning manager

**Files:**
- Create: `include/wifi_provisioning_manager.h`
- Create: `src/wifi_provisioning_manager.cpp`
- Modify: `platformio.ini`
- Modify: `tests/test_ota_project_config.py`

**Interfaces:**
- Consumes: Task 1 state and gesture functions; legacy `FRIDGE_WIFI_SSID` and `FRIDGE_WIFI_PASSWORD` from `ota_config.h`.
- Produces: global `wifiProvisioningManager` with `begin(now)`, `poll(now)`, `pollButtons(bothPressed,otaUpdating,now)`, `stateName()`, `isPortalActive()`, `connected()`, `ipAddress()`, and `apSsid()`.

- [x] **Step 1: Extend the repository policy test and verify RED**

Require `tzapu/WiFiManager @ 2.0.17` in `platformio.ini`, require `WIFI_PORTAL_TIMEOUT_MS`, and reject calls to `autoConnect(` in `src/`. Run `python -m unittest tests.test_ota_project_config -v`; expected failure is the missing dependency/manager.

- [x] **Step 2: Add the fixed dependency and manager**

Create one global WiFiManager, call `setConfigPortalBlocking(false)`, `setConfigPortalTimeout(60)`, `setDebugOutput(false)`, and explicitly call `startConfigPortal(apSsid,apPassword)` only for Task 1-approved triggers. Call `process()` only while in `Portal`. Generate `Fridge-Setup-%06X` from the device identifier and a per-session `F%08X!` password from `esp_random()`.

- [x] **Step 3: Implement boot and retry behavior**

Use saved ESP32 credentials first, then the two non-empty legacy macros. With neither, start one portal session. For connection failure, enter `RetryWait` and call `WiFi.begin()` again only when `wifiRetryDue`; never call `startConfigPortal`. After portal timeout, call `stopConfigPortal()` and remain `UnconfiguredIdle` for the rest of that boot.

- [x] **Step 4: Implement manual reset behavior**

When the pure gesture emits `triggerPortal`, call WiFiManager `resetSettings()` to clear only Wi-Fi credentials, disconnect STA, and start a fresh 60-second portal. Ignore the request while OTA is updating.

- [x] **Step 5: Verify and commit**

Run the policy tests and the release build, then commit `feat: add non-blocking Wi-Fi provisioning manager`.

### Task 3: Decouple OTA from credentials and Wi-Fi retries

**Files:**
- Modify: `src/ota_manager.cpp`
- Modify: `include/ota_manager.h`
- Modify: `include/ota_config.example.h`
- Modify: `docs/ota.md`

**Interfaces:**
- Consumes: `WiFi.status()` managed by Task 2.
- Preserves: `OtaManager::begin`, `poll`, `isUpdating`, status/HIL accessors, password authentication, compressor safe-stop callback, and automatic reboot after success.

- [x] **Step 1: Add a source-policy assertion and verify RED**

Extend `tests/test_ota_project_config.py` to reject `WiFi.begin(` and `WiFi.disconnect(` inside `src/ota_manager.cpp`, proving network ownership belongs only to `WifiProvisioningManager`.

- [x] **Step 2: Refactor OTA connection handling**

Make OTA configuration require only non-empty `FRIDGE_OTA_HOSTNAME` and `FRIDGE_OTA_PASSWORD`. On Wi-Fi connect, start ArduinoOTA; on disconnect, end/pause the service and wait for Task 2 to reconnect. Do not open a portal or alter saved Wi-Fi.

- [x] **Step 3: Update compatibility documentation and verify GREEN**

Document that Wi-Fi macros are optional migration inputs, OTA credentials remain local, and first USB firmware can be provisioned by phone. Run policy tests and release/OTA builds, then commit `refactor: separate OTA from Wi-Fi provisioning`.

### Task 4: Main-loop gesture, OLED, and HIL integration

**Files:**
- Modify: `src/main.cpp`
- Modify: `hil/dashboard.py`
- Modify: `hil/tests/test_dashboard.py`
- Modify: `tests/test_screen_timeout.py`

**Interfaces:**
- Consumes: Task 2 manager and Task 3 OTA state.
- Produces HIL fields: `wifi_state`, `wifi_provisioning`, `wifi_ap_ssid`; preserves `wifi_connected` and `ota_ip`.

- [x] **Step 1: Extend Python/source regressions and verify RED**

Require all new fields in `disconnected_snapshot()`/`Dashboard.FIELDS`. Require main-loop source to sample both active-low pins before button ticks, skip both `tick()` calls while `consumeButtons` is true, and call `pollButtons(..., otaManager.isUpdating(), ...)`.

- [x] **Step 2: Integrate startup and loop polling**

Call `wifiProvisioningManager.begin(millis())` before `otaManager.begin(...)`. Each loop polls Wi-Fi, samples both buttons, advances the gesture, and suppresses OneButton processing during a dual hold. Temperature control and fan timer continue on every loop.

- [x] **Step 3: Add portal OLED page**

While portal is active, keep OLED on and show `WiFi Setup`, AP SSID, temporary password, and `192.168.4.1`. On timeout/success force the cached normal screen to redraw. OTA progress remains higher priority if already updating, although portal entry is forbidden during OTA.

- [x] **Step 4: Add safe HIL telemetry and verify GREEN**

Expose state, boolean, and AP SSID only; never expose either password or home SSID. Run both Python suites and HIL/release builds, then commit `feat: integrate Wi-Fi provisioning controls and status`.

### Task 5: Documentation, full verification, and publication

**Files:**
- Modify: `README.md`
- Create: `docs/wifi-provisioning.md`
- Modify: `docs/hil-testing.md`
- Modify: `docs/ota.md`
- Modify: `docs/superpowers/plans/2026-07-18-wifi-provisioning.md`

**Interfaces:**
- Documents: first-boot 60-second portal, temporary AP password, `192.168.4.1`, five-second dual-button reset, no failure-triggered portal, next-boot behavior, OTA interaction, and hardware test limits.

- [x] **Step 1: Complete user and troubleshooting documentation**

Include exact phone steps, the one-minute deadline, recovery after a missed window, router replacement, wrong-password behavior, and the statement that refrigeration continues throughout provisioning.

- [x] **Step 2: Run fresh software verification**

Run `python -m unittest discover -s tests -v`, `python -m unittest discover -s hil/tests -v`, `git diff --check`, and verify no local credential file is tracked.

- [x] **Step 3: Clean-build all environments and check partition margin**

Run clean builds for `esp32-c3-devkitm-1`, `esp32-c3-hil`, and `esp32-c3-ota`. Record each firmware byte count and fail completion if any exceeds 1310720 bytes.

- [x] **Step 4: Review, commit, push, and verify remote**

Mark completed checkboxes, commit `docs: complete Wi-Fi provisioning guide`, push `main`, and require `git ls-remote origin refs/heads/main` to match local `HEAD`. Report captive-portal and radio behavior as awaiting physical ESP32-C3 verification.
