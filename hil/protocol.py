"""Request sequencing and response validation for the serial HIL protocol."""

import json


class ProtocolError(RuntimeError):
    pass


class HilClient:
    def __init__(self, transport):
        self.transport = transport
        self._sequence = 0

    def command(self, command, timeout=2.0):
        self._sequence += 1
        line = f"HIL {self._sequence} {command}"
        raw = self.transport.exchange(line, timeout=timeout)
        try:
            response = json.loads(raw)
        except json.JSONDecodeError as error:
            raise ProtocolError(f"invalid JSON response: {raw!r}") from error
        if response.get("sequence") != self._sequence:
            raise ProtocolError(f"sequence mismatch: expected {self._sequence}, got {response.get('sequence')}")
        if not isinstance(response.get("ok"), bool):
            raise ProtocolError("response is missing boolean 'ok'")
        return response

    def status(self):
        return self.command("STATUS")
