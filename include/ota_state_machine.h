#pragma once

#include <stdint.h>

enum class OtaState : uint8_t {
  Disabled,
  Connecting,
  Ready,
  Updating,
  Error,
};

constexpr uint32_t OTA_CONNECT_TIMEOUT_MS = 15000u;
constexpr uint32_t OTA_RETRY_INTERVAL_MS = 30000u;
constexpr uint32_t OTA_ERROR_DISPLAY_MS = 5000u;

struct OtaStatus {
  OtaState state;
  uint32_t stateStartedAt;
  uint8_t progress;
  int errorCode;

  constexpr OtaStatus(OtaState initialState, uint32_t startedAt, uint8_t initialProgress, int initialError)
      : state(initialState), stateStartedAt(startedAt), progress(initialProgress), errorCode(initialError) {}
};

constexpr OtaStatus otaInitialStatus(bool configured) {
  return OtaStatus(configured ? OtaState::Connecting : OtaState::Disabled, 0u, 0u, 0);
}

constexpr OtaStatus otaBeginConnecting(uint32_t now) {
  return OtaStatus(OtaState::Connecting, now, 0u, 0);
}

constexpr OtaStatus otaErrorStatus(uint32_t now, int errorCode) {
  return OtaStatus(OtaState::Error, now, 0u, errorCode);
}

constexpr bool otaConnectionTimedOut(const OtaStatus& status, uint32_t now) {
  return status.state == OtaState::Connecting &&
         static_cast<uint32_t>(now - status.stateStartedAt) >= OTA_CONNECT_TIMEOUT_MS;
}

constexpr bool otaRetryDue(const OtaStatus& status, uint32_t now) {
  return status.state == OtaState::Error &&
         static_cast<uint32_t>(now - status.stateStartedAt) >= OTA_RETRY_INTERVAL_MS;
}
