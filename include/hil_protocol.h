#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t HIL_MAX_LINE_LENGTH = 128;

enum class HilCommandType : uint8_t {
  Invalid,
  Ping,
  Status,
  OutputsUnlock,
  OutputsLock,
  NtcPhysical,
  NtcValue,
  NtcFault,
  DsPhysical,
  DsValue,
  DsFault,
  ButtonUpClick,
  ButtonDownClick,
  ButtonUpLong,
  ButtonDownLong,
  DisplayWake,
  NvsClear,
  Reboot,
  Reset,
};

enum class HilParseResult : uint8_t {
  Ok,
  Empty,
  TooLong,
  InvalidPrefix,
  InvalidSequence,
  UnknownCommand,
  InvalidArgument,
};

struct HilCommand {
  uint32_t sequence;
  HilCommandType type;
  float value;
  uint32_t durationMs;
};

HilParseResult parseHilCommand(const char* line, HilCommand& command);
const char* hilParseError(HilParseResult result);
