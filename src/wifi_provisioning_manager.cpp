#include "wifi_provisioning_manager.h"

#include <WiFi.h>
#include <cstring>
#include <esp_system.h>

#include "ota_config.h"

WifiProvisioningManager wifiProvisioningManager;

WifiProvisioningManager::WifiProvisioningManager()
    : status_(wifiInitialStatus(false, 0u)), gesture_(initialProvisioningGesture()) {}

bool WifiProvisioningManager::legacyCredentialsAvailable() const {
  return std::strlen(FRIDGE_WIFI_SSID) > 0 && std::strlen(FRIDGE_WIFI_PASSWORD) > 0;
}

void WifiProvisioningManager::begin(uint32_t now) {
  manager_.setConfigPortalBlocking(false);
  manager_.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_MS / 1000u);
  manager_.setSaveConnectTimeout(5u);
  manager_.setDebugOutput(false);
  manager_.setShowInfoUpdate(false);
  const char* menu[] = {"wifi", "exit"};
  manager_.setMenu(menu, 2u);
  manager_.setWebServerCallback([this]() {
    const std::function<void()> deny = [this]() {
      manager_.server->send(404, "text/plain", "Not found");
    };
    // Register these before WiFiManager's built-in handlers so the temporary
    // provisioning AP cannot be used for firmware or device management.
    manager_.server->on("/update", deny);
    manager_.server->on("/u", deny);
    manager_.server->on("/erase", deny);
    manager_.server->on("/restart", deny);
    manager_.server->on("/info", deny);
    manager_.server->on("/param", deny);
    manager_.server->on("/close", deny);
  });

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  if (std::strlen(FRIDGE_OTA_HOSTNAME) > 0) WiFi.setHostname(FRIDGE_OTA_HOSTNAME);

  if (manager_.getWiFiIsSaved()) {
    startSavedConnection(now);
  } else if (legacyCredentialsAvailable()) {
    WiFi.persistent(true);
    WiFi.begin(FRIDGE_WIFI_SSID, FRIDGE_WIFI_PASSWORD);
    status_ = wifiInitialStatus(true, now);
  } else {
    status_ = wifiInitialStatus(false, now);
    startPortal(now, false);
  }
}

void WifiProvisioningManager::generatePortalIdentity() {
  char ssid[24];
  char password[12];
  const uint32_t deviceId = static_cast<uint32_t>(ESP.getEfuseMac() & 0x00ffffffu);
  snprintf(ssid, sizeof(ssid), "Fridge-Setup-%06lX", static_cast<unsigned long>(deviceId));
  snprintf(password, sizeof(password), "F%08lX!", static_cast<unsigned long>(esp_random()));
  apSsid_ = ssid;
  apPassword_ = password;
}

void WifiProvisioningManager::startPortal(uint32_t now, bool clearCredentials) {
  if (manager_.getConfigPortalActive()) manager_.stopConfigPortal();
  if (clearCredentials) manager_.resetSettings();
  generatePortalIdentity();
  WiFi.mode(WIFI_AP_STA);
  manager_.startConfigPortal(apSsid_.c_str(), apPassword_.c_str());
  status_ = wifiPortalStatus(now);
}

void WifiProvisioningManager::stopPortal() {
  if (manager_.getConfigPortalActive()) manager_.stopConfigPortal();
  WiFi.softAPdisconnect(true);
  apSsid_ = "";
  apPassword_ = "";
}

void WifiProvisioningManager::startSavedConnection(uint32_t now) {
  stopPortal();
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  status_ = wifiInitialStatus(true, now);
}

void WifiProvisioningManager::poll(uint32_t now) {
  switch (status_.state) {
    case WifiProvisioningState::NoCredentials:
      startPortal(now, false);
      return;
    case WifiProvisioningState::UnconfiguredIdle:
      return;
    case WifiProvisioningState::Connecting:
      if (WiFi.status() == WL_CONNECTED) {
        status_ = WifiProvisioningStatus(WifiProvisioningState::Connected, now);
      } else if (wifiConnectionTimedOut(status_, now)) {
        WiFi.disconnect(false, false);
        status_ = wifiRetryStatus(now);
      }
      return;
    case WifiProvisioningState::Connected:
      if (WiFi.status() != WL_CONNECTED) status_ = wifiRetryStatus(now);
      return;
    case WifiProvisioningState::RetryWait:
      if (wifiRetryDue(status_, now)) startSavedConnection(now);
      return;
    case WifiProvisioningState::Portal:
      manager_.process();
      if (WiFi.status() == WL_CONNECTED) {
        stopPortal();
        status_ = WifiProvisioningStatus(WifiProvisioningState::Connected, now);
      } else if (wifiPortalTimedOut(status_, now) || !manager_.getConfigPortalActive()) {
        stopPortal();
        status_ = wifiAfterPortalTimeout(status_, now);
      }
      return;
  }
}

bool WifiProvisioningManager::pollButtons(bool bothPressed, bool otaUpdating, uint32_t now) {
  const ProvisioningGestureResult result =
      advanceProvisioningGesture(gesture_, bothPressed, otaUpdating, now);
  gesture_ = result.gesture;
  if (result.triggerPortal) startPortal(now, true);
  return result.consumeButtons;
}

const char* WifiProvisioningManager::stateName() const {
  switch (status_.state) {
    case WifiProvisioningState::NoCredentials: return "no_credentials";
    case WifiProvisioningState::UnconfiguredIdle: return "unconfigured_idle";
    case WifiProvisioningState::Connecting: return "connecting";
    case WifiProvisioningState::Connected: return "connected";
    case WifiProvisioningState::RetryWait: return "retry_wait";
    case WifiProvisioningState::Portal: return "portal";
  }
  return "unknown";
}

bool WifiProvisioningManager::connected() const {
  return WiFi.status() == WL_CONNECTED;
}

String WifiProvisioningManager::ipAddress() const {
  return connected() ? WiFi.localIP().toString() : String();
}
