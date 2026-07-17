#ifdef FRIDGE_HIL
#include "hil_protocol.h"

static_assert(static_cast<unsigned>(HilCommandType::Ping) != static_cast<unsigned>(HilCommandType::Status),
              "HIL commands must have distinct values");
static_assert(HIL_MAX_LINE_LENGTH >= 64, "HIL command buffer must hold documented commands");
#endif
