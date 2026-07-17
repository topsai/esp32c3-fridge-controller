#ifdef FRIDGE_HIL

#include "hil_protocol.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {

bool parseUint32(const char* text, uint32_t& value) {
  if (text == nullptr || *text == '\0' || *text == '-') return false;
  char* end = nullptr;
  unsigned long parsed = strtoul(text, &end, 10);
  if (*end != '\0') return false;
  value = static_cast<uint32_t>(parsed);
  return true;
}

bool parseFloat(const char* text, float& value) {
  if (text == nullptr || *text == '\0') return false;
  char* end = nullptr;
  value = strtof(text, &end);
  return *end == '\0' && isfinite(value) && value >= -100.0f && value <= 150.0f;
}

}  // namespace

HilParseResult parseHilCommand(const char* line, HilCommand& command) {
  command = { 0u, HilCommandType::Invalid, 0.0f, 0u };
  if (line == nullptr || *line == '\0') return HilParseResult::Empty;
  if (strlen(line) >= HIL_MAX_LINE_LENGTH) return HilParseResult::TooLong;

  char buffer[HIL_MAX_LINE_LENGTH];
  strcpy(buffer, line);
  char* tokens[7] = {};
  size_t count = 0;
  char* save = nullptr;
  for (char* token = strtok_r(buffer, " \t\r\n", &save); token != nullptr && count < 7;
       token = strtok_r(nullptr, " \t\r\n", &save)) {
    tokens[count++] = token;
  }
  if (count == 0) return HilParseResult::Empty;
  if (strcmp(tokens[0], "HIL") != 0) return HilParseResult::InvalidPrefix;
  if (count < 3 || !parseUint32(tokens[1], command.sequence)) return HilParseResult::InvalidSequence;

  const char* verb = tokens[2];
  if (strcmp(verb, "PING") == 0 && count == 3) command.type = HilCommandType::Ping;
  else if (strcmp(verb, "STATUS") == 0 && count == 3) command.type = HilCommandType::Status;
  else if (strcmp(verb, "DISPLAY") == 0 && count == 4 && strcmp(tokens[3], "WAKE") == 0) command.type = HilCommandType::DisplayWake;
  else if (strcmp(verb, "REBOOT") == 0 && count == 3) command.type = HilCommandType::Reboot;
  else if (strcmp(verb, "RESET") == 0 && count == 3) command.type = HilCommandType::Reset;
  else if (strcmp(verb, "NVS") == 0 && count == 4 && strcmp(tokens[3], "CLEAR") == 0) command.type = HilCommandType::NvsClear;
  else if (strcmp(verb, "OUTPUTS") == 0 && count == 4) {
    if (strcmp(tokens[3], "UNLOCK") == 0) command.type = HilCommandType::OutputsUnlock;
    else if (strcmp(tokens[3], "LOCK") == 0) command.type = HilCommandType::OutputsLock;
    else return HilParseResult::InvalidArgument;
  } else if ((strcmp(verb, "NTC") == 0 || strcmp(verb, "DS") == 0) && count >= 4) {
    const bool ntc = strcmp(verb, "NTC") == 0;
    if (count == 4 && strcmp(tokens[3], "PHYSICAL") == 0) command.type = ntc ? HilCommandType::NtcPhysical : HilCommandType::DsPhysical;
    else if (count == 4 && strcmp(tokens[3], "FAULT") == 0) command.type = ntc ? HilCommandType::NtcFault : HilCommandType::DsFault;
    else if (count == 5 && strcmp(tokens[3], "VALUE") == 0 && parseFloat(tokens[4], command.value)) command.type = ntc ? HilCommandType::NtcValue : HilCommandType::DsValue;
    else return HilParseResult::InvalidArgument;
  } else if (strcmp(verb, "BUTTON") == 0 && count >= 5) {
    const bool up = strcmp(tokens[3], "UP") == 0;
    const bool down = strcmp(tokens[3], "DOWN") == 0;
    if (!up && !down) return HilParseResult::InvalidArgument;
    if (count == 5 && strcmp(tokens[4], "CLICK") == 0) command.type = up ? HilCommandType::ButtonUpClick : HilCommandType::ButtonDownClick;
    else if (count == 6 && strcmp(tokens[4], "LONG") == 0 && parseUint32(tokens[5], command.durationMs)
             && command.durationMs >= 200u && command.durationMs <= 30000u) command.type = up ? HilCommandType::ButtonUpLong : HilCommandType::ButtonDownLong;
    else return HilParseResult::InvalidArgument;
  } else {
    return HilParseResult::UnknownCommand;
  }

  return command.type == HilCommandType::Invalid ? HilParseResult::UnknownCommand : HilParseResult::Ok;
}

const char* hilParseError(HilParseResult result) {
  switch (result) {
    case HilParseResult::Ok: return "ok";
    case HilParseResult::Empty: return "empty";
    case HilParseResult::TooLong: return "line_too_long";
    case HilParseResult::InvalidPrefix: return "invalid_prefix";
    case HilParseResult::InvalidSequence: return "invalid_sequence";
    case HilParseResult::UnknownCommand: return "unknown_command";
    case HilParseResult::InvalidArgument: return "invalid_argument";
  }
  return "unknown_error";
}

#endif
