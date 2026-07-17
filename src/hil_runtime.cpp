#ifdef FRIDGE_HIL

#include "hil_runtime.h"
#include <string.h>

namespace {
HilRuntimeState state;
char lineBuffer[128];
size_t lineLength = 0;
bool discarding = false;
}

HilRuntimeState& hilRuntime() {
  return state;
}

void resetHilRuntime(uint32_t now) {
  state = {
    HilInputMode::Physical,
    HilInputMode::Physical,
    0.0f,
    0.0f,
    false,
    now,
    0u,
    0,
    0u,
    0u,
  };
  lineLength = 0;
  discarding = false;
}

bool readHilLine(Stream& stream, char* destination, size_t capacity) {
  while (stream.available() > 0) {
    const char value = static_cast<char>(stream.read());
    if (value == '\r') continue;
    if (value == '\n') {
      if (discarding) {
        discarding = false;
        lineLength = 0;
        destination[0] = '\0';
        return true;
      }
      if (lineLength == 0) continue;
      lineBuffer[lineLength] = '\0';
      strncpy(destination, lineBuffer, capacity - 1);
      destination[capacity - 1] = '\0';
      lineLength = 0;
      return true;
    }
    if (discarding) continue;
    if (lineLength + 1 >= sizeof(lineBuffer)) {
      discarding = true;
      continue;
    }
    lineBuffer[lineLength++] = value;
  }
  return false;
}

#endif
