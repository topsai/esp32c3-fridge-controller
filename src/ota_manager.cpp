#include "ota_manager.h"

#include <ArduinoOTA.h>
#include <WiFi.h>
#include <cstring>

#include "ota_config.h"

OtaManager otaManager;

OtaManager::OtaManager()
    : status_(otaInitialStatus(false)), onStart_(nullptr), serviceStarted_(false) {}

bool OtaManager::configured() const {
  return std::strlen(FRIDGE_WIFI_SSID) > 0 &&
         std::strlen(FRIDGE_WIFI_PASSWORD) > 0 &&
         std::strlen(FRIDGE_OTA_HOSTNAME) > 0 &&
         std::strlen(FRIDGE_OTA_PASSWORD) > 0;
}

void OtaManager::begin(uint32_t now, OtaStartCallback onStart) {
  onStart_ = onStart;
  if (!configured()) {
    status_ = otaInitialStatus(false);
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(FRIDGE_OTA_HOSTNAME);
  startConnection(now);
}

void OtaManager::startConnection(uint32_t now) {
  if (serviceStarted_) {
    ArduinoOTA.end();
    serviceStarted_ = false;
  }
  WiFi.disconnect();
  WiFi.begin(FRIDGE_WIFI_SSID, FRIDGE_WIFI_PASSWORD);
  status_ = otaBeginConnecting(now);
}

void OtaManager::startService(uint32_t now) {
  ArduinoOTA.setHostname(FRIDGE_OTA_HOSTNAME);
  ArduinoOTA.setPassword(FRIDGE_OTA_PASSWORD);
  ArduinoOTA.onStart([this]() {
    status_ = OtaStatus(OtaState::Updating, millis(), 0u, 0);
    if (onStart_ != nullptr) onStart_();
  });
  ArduinoOTA.onEnd([this]() {
    status_ = OtaStatus(OtaState::Updating, millis(), 100u, 0);
  });
  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    status_.progress = total == 0u
                           ? 0u
                           : static_cast<uint8_t>((static_cast<uint64_t>(progress) * 100u) / total);
  });
  ArduinoOTA.onError([this](ota_error_t error) {
    enterError(millis(), static_cast<int>(error));
  });
  ArduinoOTA.begin();
  serviceStarted_ = true;
  status_ = OtaStatus(OtaState::Ready, now, 0u, 0);
}

void OtaManager::enterError(uint32_t now, int errorCode) {
  status_ = otaErrorStatus(now, errorCode);
}

void OtaManager::poll(uint32_t now) {
  switch (status_.state) {
    case OtaState::Disabled:
      return;
    case OtaState::Connecting:
      if (WiFi.status() == WL_CONNECTED) startService(now);
      else if (otaConnectionTimedOut(status_, now)) {
        WiFi.disconnect();
        enterError(now, -1);
      }
      return;
    case OtaState::Ready:
      if (WiFi.status() != WL_CONNECTED) {
        enterError(now, -2);
        return;
      }
      ArduinoOTA.handle();
      return;
    case OtaState::Updating:
      if (WiFi.status() != WL_CONNECTED) {
        enterError(now, -3);
        return;
      }
      ArduinoOTA.handle();
      return;
    case OtaState::Error:
      if (WiFi.status() == WL_CONNECTED && serviceStarted_ &&
          static_cast<uint32_t>(now - status_.stateStartedAt) >= OTA_ERROR_DISPLAY_MS) {
        status_ = OtaStatus(OtaState::Ready, now, 0u, 0);
        ArduinoOTA.handle();
      } else if (otaRetryDue(status_, now)) {
        startConnection(now);
      }
      return;
  }
}

const char* OtaManager::stateName() const {
  switch (status_.state) {
    case OtaState::Disabled: return "disabled";
    case OtaState::Connecting: return "connecting";
    case OtaState::Ready: return "ready";
    case OtaState::Updating: return "updating";
    case OtaState::Error: return "error";
  }
  return "unknown";
}

bool OtaManager::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

String OtaManager::ipAddress() const {
  return wifiConnected() ? WiFi.localIP().toString() : String();
}
