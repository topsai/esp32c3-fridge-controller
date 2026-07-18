#pragma once

#include <Arduino.h>
#include "ota_state_machine.h"

using OtaStartCallback = void (*)();

class OtaManager {
 public:
  OtaManager();

  void begin(uint32_t now, OtaStartCallback onStart);
  void poll(uint32_t now);

  OtaState state() const { return status_.state; }
  const char* stateName() const;
  bool isUpdating() const { return status_.state == OtaState::Updating; }
  bool wifiConnected() const;
  uint8_t progress() const { return status_.progress; }
  int errorCode() const { return status_.errorCode; }
  String ipAddress() const;

 private:
  bool configured() const;
  void startConnection(uint32_t now);
  void startService(uint32_t now);
  void enterError(uint32_t now, int errorCode);

  OtaStatus status_;
  OtaStartCallback onStart_;
  bool serviceStarted_;
};

extern OtaManager otaManager;
