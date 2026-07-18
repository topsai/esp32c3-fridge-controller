#include "wifi_provisioning_state.h"

namespace {

constexpr WifiProvisioningStatus noCredentials = wifiInitialStatus(false, 100u);
static_assert(noCredentials.state == WifiProvisioningState::NoCredentials,
              "first boot without credentials must offer provisioning");

constexpr WifiProvisioningStatus connecting = wifiInitialStatus(true, 100u);
static_assert(connecting.state == WifiProvisioningState::Connecting,
              "saved credentials must start a station connection");
static_assert(wifiConnectionTimedOut(connecting, 100u + WIFI_CONNECT_TIMEOUT_MS),
              "station connection must time out at its deadline");

constexpr WifiProvisioningStatus portal = wifiPortalStatus(200u);
static_assert(!wifiPortalTimedOut(portal, 200u + WIFI_PORTAL_TIMEOUT_MS - 1u),
              "portal must remain open for the complete one-minute window");
static_assert(wifiPortalTimedOut(portal, 200u + WIFI_PORTAL_TIMEOUT_MS),
              "portal must close at one minute");
constexpr WifiProvisioningStatus idle = wifiAfterPortalTimeout(portal, 200u + WIFI_PORTAL_TIMEOUT_MS);
static_assert(idle.state == WifiProvisioningState::UnconfiguredIdle,
              "an expired first-boot portal must stay closed for this boot");

constexpr WifiProvisioningStatus retry = wifiRetryStatus(0xfffffff0u);
static_assert(!wifiRetryDue(retry, 0xfffffff0u + WIFI_RETRY_INTERVAL_MS - 1u),
              "retry must not start early");
static_assert(wifiRetryDue(retry, 0xfffffff0u + WIFI_RETRY_INTERVAL_MS),
              "retry must work across millis wraparound");
constexpr WifiProvisioningStatus retryConnecting = wifiBeginConnecting(retry, 500u);
static_assert(retryConnecting.state == WifiProvisioningState::Connecting,
              "network failure retries STA and never opens the portal");

constexpr ProvisioningGesture gesture0 = initialProvisioningGesture();
constexpr ProvisioningGestureResult gesturePressed =
    advanceProvisioningGesture(gesture0, true, false, 100u);
static_assert(gesturePressed.consumeButtons && !gesturePressed.triggerPortal,
              "dual press must suppress temperature buttons immediately");
constexpr ProvisioningGestureResult gesture4999 =
    advanceProvisioningGesture(gesturePressed.gesture, true, false, 5099u);
static_assert(!gesture4999.triggerPortal, "4999 ms hold must not trigger provisioning");
constexpr ProvisioningGestureResult gesture5000 =
    advanceProvisioningGesture(gesture4999.gesture, true, false, 5100u);
static_assert(gesture5000.triggerPortal, "5000 ms hold must trigger provisioning once");
constexpr ProvisioningGestureResult gestureRepeated =
    advanceProvisioningGesture(gesture5000.gesture, true, false, 9000u);
static_assert(!gestureRepeated.triggerPortal, "continued hold must not retrigger provisioning");
constexpr ProvisioningGestureResult gestureReleased =
    advanceProvisioningGesture(gestureRepeated.gesture, false, false, 9001u);
static_assert(gestureReleased.gesture.armed, "release must rearm the gesture");

constexpr ProvisioningGestureResult otaBlocked =
    advanceProvisioningGesture(gesture0, true, true, 100u);
constexpr ProvisioningGestureResult otaStillHeld =
    advanceProvisioningGesture(otaBlocked.gesture, true, false, 10000u);
static_assert(!otaBlocked.triggerPortal && !otaStillHeld.triggerPortal,
              "OTA must block provisioning until both buttons are released");

}  // namespace
