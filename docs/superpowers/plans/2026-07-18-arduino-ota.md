# Arduino OTA Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add password-protected, non-blocking ArduinoOTA updates without changing normal refrigerator control behavior.

**Architecture:** A pure transition contract defines OTA states and retry timing. `OtaManager` owns Wi-Fi and ArduinoOTA runtime behavior; `main.cpp` only supplies the safe-stop callback, polls the manager, blocks compressor restart during an upload, and exposes status to OLED/HIL.

**Tech Stack:** PlatformIO, Arduino ESP32, ArduinoOTA, WiFi, C++11 static assertions, Python unittest/Tkinter.

## Global Constraints

- Keep existing thermostat, compressor/fan state machine, button, sensor, NVS, and HIL behavior unchanged outside OTA integration.
- Credentials live only in ignored `include/ota_config.local.h`; OTA is disabled when configuration is absent or empty.
- OTA requires a non-empty password and stops the compressor through `setRelayPhysical(false)` before accepting firmware data.
- Wi-Fi connection and retries are non-blocking; the main loop continues to run.
- The first OTA-capable firmware installation is performed by USB.

---

### Task 1: OTA state transition contract

**Files:**
- Create: `src/ota_state_machine_contract.cpp`
- Create: `include/ota_state_machine.h`

**Interfaces:**
- Produces: `OtaState`, `OtaStatus`, `otaInitialStatus()`, `otaBeginConnecting()`, `otaConnectionTimedOut()`, `otaRetryDue()`.

- [ ] Write compile-time assertions for disabled startup, connecting transition, timeout, and wrap-safe retry timing.
- [ ] Run `pio run -e esp32-c3-devkitm-1` and verify failure because `ota_state_machine.h` is missing.
- [ ] Implement the smallest pure header satisfying those assertions.
- [ ] Re-run the build and verify the contract passes.

### Task 2: Non-blocking ArduinoOTA runtime

**Files:**
- Create: `include/ota_config.example.h`
- Create: `include/ota_config.h`
- Create: `include/ota_manager.h`
- Create: `src/ota_manager.cpp`
- Modify: `.gitignore`
- Modify: `platformio.ini`

**Interfaces:**
- Consumes: OTA transition contract from Task 1.
- Produces: global `OtaManager` API: `begin(now, callback)`, `poll(now)`, `state()`, `stateName()`, `isUpdating()`, `wifiConnected()`, `progress()`, `errorCode()`, `ipAddress()`.

- [ ] Add a Python repository-policy test requiring the local config to be ignored, example keys to exist, and the OTA upload environment to use `espota`; run it and verify failure.
- [ ] Add optional configuration defaults, ignored local secrets, `esp32-c3-ota`, and the non-blocking manager with ArduinoOTA callbacks.
- [ ] Run the policy test and all three PlatformIO builds.

### Task 3: Safe controller, OLED, and HIL integration

**Files:**
- Modify: `src/main.cpp`
- Modify: `hil/dashboard.py`
- Modify: `hil/tests/test_dashboard.py`

**Interfaces:**
- Consumes: `OtaManager` from Task 2.
- Produces: safe OTA start, OTA progress OLED, and HIL keys `ota_state`, `ota_progress`, `wifi_connected`, `ota_ip`.

- [ ] Extend the dashboard test first and verify it fails because OTA fields are absent.
- [ ] Poll OTA from the normal loop, stop/lock out the compressor during upload, and render progress without blocking fan cooldown.
- [ ] Add OTA telemetry to STATUS and make the always-open dashboard show the new fields when disconnected or connected.
- [ ] Run Python tests and HIL/release builds.

### Task 4: Documentation and release verification

**Files:**
- Create: `docs/ota.md`
- Modify: `README.md`
- Modify: `docs/hil.md`

**Interfaces:**
- Documents USB bootstrap, local secrets, serial upload, network upload, hostname discovery fallback, safety behavior, troubleshooting, rollback limitations, and HIL telemetry.

- [ ] Document configuration and operational procedures without publishing secrets.
- [ ] Run `python -m unittest discover -s tests -v` and `python -m unittest discover -s hil/tests -v`.
- [ ] Clean-build release, HIL, and OTA environments and inspect firmware sizes.
- [ ] Verify `include/ota_config.local.h` is ignored and absent from tracked files.
- [ ] Review the diff, commit intentionally, push `main`, and verify the remote commit.
