#include "ota_state_machine.h"

constexpr OtaStatus kDisabled = otaInitialStatus(false);
static_assert(kDisabled.state == OtaState::Disabled, "missing config must disable OTA");

constexpr OtaStatus kConnecting = otaBeginConnecting(1000u);
static_assert(kConnecting.state == OtaState::Connecting, "configured OTA must start connecting");
static_assert(!otaConnectionTimedOut(kConnecting, 1000u + OTA_CONNECT_TIMEOUT_MS - 1u),
              "connection timeout must not fire early");
static_assert(otaConnectionTimedOut(kConnecting, 1000u + OTA_CONNECT_TIMEOUT_MS),
              "connection timeout must fire at the deadline");

constexpr OtaStatus kError = otaErrorStatus(0xfffffff0u, -1);
static_assert(!otaRetryDue(kError, 0xfffffff0u + OTA_RETRY_INTERVAL_MS - 1u),
              "retry must not fire early");
static_assert(otaRetryDue(kError, 0xfffffff0u + OTA_RETRY_INTERVAL_MS),
              "retry timing must remain correct across millis wraparound");
