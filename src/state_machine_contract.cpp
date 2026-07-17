#include "fridge_state_machine.h"

namespace {

constexpr FridgeOutputs initial = initialFridgeOutputs();
static_assert(initial.compressor == CompressorState::Off, "compressor starts off");
static_assert(initial.fan == FanState::Off, "fan starts off");

constexpr FridgeOutputs running = requestCompressor(initial, true, 1000u);
static_assert(running.compressor == CompressorState::Running, "run request starts compressor");
static_assert(running.fan == FanState::FollowingCompressor, "fan follows running compressor");

constexpr FridgeOutputs cooling = requestCompressor(running, false, 2000u);
static_assert(cooling.compressor == CompressorState::Off, "stop request stops compressor");
static_assert(cooling.fan == FanState::Cooldown, "fan enters cooldown after stop");
static_assert(cooling.cooldownStartedAt == 2000u, "stop transition records cooldown start");

constexpr FridgeOutputs repeatedStop = requestCompressor(cooling, false, 9000u);
static_assert(repeatedStop.cooldownStartedAt == 2000u, "repeated stop does not extend cooldown");

constexpr FridgeOutputs beforeDeadline = advanceFanTimer(cooling, 301999u);
static_assert(beforeDeadline.fan == FanState::Cooldown, "fan runs through 299999 ms");

constexpr FridgeOutputs atDeadline = advanceFanTimer(cooling, 302000u);
static_assert(atDeadline.fan == FanState::Off, "fan stops at 300000 ms");

constexpr FridgeOutputs restarted = requestCompressor(cooling, true, 5000u);
static_assert(restarted.compressor == CompressorState::Running, "cooldown can restart compressor");
static_assert(restarted.fan == FanState::FollowingCompressor, "restart cancels cooldown state");

constexpr uint32_t wrapStartTime = 0xFFFFFF00u;
constexpr FridgeOutputs wrapCooling = requestCompressor(running, false, wrapStartTime);
constexpr FridgeOutputs wrapBefore = advanceFanTimer(wrapCooling, wrapStartTime + 299999u);
constexpr FridgeOutputs wrapDone = advanceFanTimer(wrapCooling, wrapStartTime + 300000u);
static_assert(wrapBefore.fan == FanState::Cooldown, "cooldown survives millis wrap");
static_assert(wrapDone.fan == FanState::Off, "cooldown expires across millis wrap");

}  // namespace
