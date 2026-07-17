#pragma once

#include <stdint.h>

enum class CompressorState : uint8_t {
  Off,
  Running,
};

enum class FanState : uint8_t {
  Off,
  FollowingCompressor,
  Cooldown,
};

struct FridgeOutputs {
  CompressorState compressor;
  FanState fan;
  uint32_t cooldownStartedAt;
};

constexpr uint32_t FAN_COOLDOWN_MS = 300000UL;

constexpr FridgeOutputs initialFridgeOutputs() {
  return { CompressorState::Off, FanState::Off, 0u };
}

constexpr FridgeOutputs requestCompressor(FridgeOutputs current, bool run, uint32_t now) {
  return run
           ? FridgeOutputs{ CompressorState::Running, FanState::FollowingCompressor, current.cooldownStartedAt }
           : (current.compressor == CompressorState::Running
                ? FridgeOutputs{ CompressorState::Off, FanState::Cooldown, now }
                : current);
}

constexpr FridgeOutputs advanceFanTimer(FridgeOutputs current, uint32_t now) {
  return current.fan == FanState::Cooldown
             && static_cast<uint32_t>(now - current.cooldownStartedAt) >= FAN_COOLDOWN_MS
           ? FridgeOutputs{ CompressorState::Off, FanState::Off, current.cooldownStartedAt }
           : current;
}
