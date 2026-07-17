#pragma once

#ifdef FRIDGE_HIL

#include <Arduino.h>
#include <stdint.h>

enum class HilInputMode : uint8_t {
  Physical,
  InjectedValue,
  InjectedFault,
};

struct HilRuntimeState {
  HilInputMode ntcMode;
  HilInputMode dsMode;
  float ntcValue;
  float dsValue;
  bool outputsUnlocked;
  uint32_t lastCommandAt;
  uint32_t lastSequence;
  int8_t longPressDirection;
  uint16_t longPressRemaining;
  uint32_t nextLongPressAt;
};

constexpr uint32_t HIL_WATCHDOG_MS = 10000UL;

HilRuntimeState& hilRuntime();
void resetHilRuntime(uint32_t now);
bool readHilLine(Stream& stream, char* destination, size_t capacity);

#endif
