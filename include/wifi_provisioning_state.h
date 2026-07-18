#pragma once

#include <stdint.h>

enum class WifiProvisioningState : uint8_t {
  NoCredentials,
  UnconfiguredIdle,
  Connecting,
  Connected,
  RetryWait,
  Portal,
};

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000u;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 30000u;
constexpr uint32_t WIFI_PORTAL_TIMEOUT_MS = 60000u;
constexpr uint32_t WIFI_PROVISIONING_HOLD_MS = 5000u;

struct WifiProvisioningStatus {
  WifiProvisioningState state;
  uint32_t stateStartedAt;

  constexpr WifiProvisioningStatus(WifiProvisioningState initialState, uint32_t startedAt)
      : state(initialState), stateStartedAt(startedAt) {}
};

constexpr WifiProvisioningStatus wifiInitialStatus(bool hasCredentials, uint32_t now) {
  return WifiProvisioningStatus(hasCredentials ? WifiProvisioningState::Connecting
                                               : WifiProvisioningState::NoCredentials,
                                now);
}

constexpr WifiProvisioningStatus wifiPortalStatus(uint32_t now) {
  return WifiProvisioningStatus(WifiProvisioningState::Portal, now);
}

constexpr WifiProvisioningStatus wifiAfterPortalTimeout(const WifiProvisioningStatus&, uint32_t now) {
  return WifiProvisioningStatus(WifiProvisioningState::UnconfiguredIdle, now);
}

constexpr WifiProvisioningStatus wifiRetryStatus(uint32_t now) {
  return WifiProvisioningStatus(WifiProvisioningState::RetryWait, now);
}

constexpr WifiProvisioningStatus wifiBeginConnecting(const WifiProvisioningStatus&, uint32_t now) {
  return WifiProvisioningStatus(WifiProvisioningState::Connecting, now);
}

constexpr bool wifiConnectionTimedOut(const WifiProvisioningStatus& status, uint32_t now) {
  return status.state == WifiProvisioningState::Connecting &&
         static_cast<uint32_t>(now - status.stateStartedAt) >= WIFI_CONNECT_TIMEOUT_MS;
}

constexpr bool wifiPortalTimedOut(const WifiProvisioningStatus& status, uint32_t now) {
  return status.state == WifiProvisioningState::Portal &&
         static_cast<uint32_t>(now - status.stateStartedAt) >= WIFI_PORTAL_TIMEOUT_MS;
}

constexpr bool wifiRetryDue(const WifiProvisioningStatus& status, uint32_t now) {
  return status.state == WifiProvisioningState::RetryWait &&
         static_cast<uint32_t>(now - status.stateStartedAt) >= WIFI_RETRY_INTERVAL_MS;
}

struct ProvisioningGesture {
  bool armed;
  bool holding;
  uint32_t heldSince;

  constexpr ProvisioningGesture(bool initialArmed, bool initialHolding, uint32_t initialHeldSince)
      : armed(initialArmed), holding(initialHolding), heldSince(initialHeldSince) {}
};

struct ProvisioningGestureResult {
  ProvisioningGesture gesture;
  bool triggerPortal;
  bool consumeButtons;

  constexpr ProvisioningGestureResult(ProvisioningGesture nextGesture, bool trigger, bool consume)
      : gesture(nextGesture), triggerPortal(trigger), consumeButtons(consume) {}
};

constexpr ProvisioningGesture initialProvisioningGesture() {
  return ProvisioningGesture(true, false, 0u);
}

constexpr ProvisioningGestureResult advanceProvisioningGesture(
    ProvisioningGesture current, bool bothPressed, bool otaUpdating, uint32_t now) {
  return !bothPressed
             ? ProvisioningGestureResult(initialProvisioningGesture(), false, false)
             : otaUpdating
                   ? ProvisioningGestureResult(ProvisioningGesture(false, true, now), false, true)
                   : !current.armed
                         ? ProvisioningGestureResult(current, false, true)
                         : !current.holding
                               ? ProvisioningGestureResult(ProvisioningGesture(true, true, now), false, true)
                               : static_cast<uint32_t>(now - current.heldSince) >= WIFI_PROVISIONING_HOLD_MS
                                     ? ProvisioningGestureResult(
                                           ProvisioningGesture(false, true, current.heldSince), true, true)
                                     : ProvisioningGestureResult(current, false, true);
}
