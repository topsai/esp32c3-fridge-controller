#pragma once

#include <Arduino.h>
#include <WiFiManager.h>

#include "wifi_provisioning_state.h"

class WifiProvisioningManager {
 public:
  WifiProvisioningManager();

  void begin(uint32_t now);
  void poll(uint32_t now);
  bool pollButtons(bool bothPressed, bool otaUpdating, uint32_t now);

  WifiProvisioningState state() const { return status_.state; }
  const char* stateName() const;
  bool isPortalActive() const { return status_.state == WifiProvisioningState::Portal; }
  bool connected() const;
  String ipAddress() const;
  const String& apSsid() const { return apSsid_; }
  const String& apPassword() const { return apPassword_; }

 private:
  bool legacyCredentialsAvailable() const;
  void startSavedConnection(uint32_t now);
  void startPortal(uint32_t now, bool clearCredentials);
  void stopPortal();
  void clearCredentialsAfterFailedPortal();
  void generatePortalIdentity();

  WiFiManager manager_;
  WifiProvisioningStatus status_;
  ProvisioningGesture gesture_;
  String apSsid_;
  String apPassword_;
};

extern WifiProvisioningManager wifiProvisioningManager;
