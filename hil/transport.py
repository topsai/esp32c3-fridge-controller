"""PySerial transport with line filtering and deterministic timeouts."""

import time


class SerialTransport:
    def __init__(self, port, baudrate=115200):
        try:
            import serial
        except ImportError as error:
            raise RuntimeError("pyserial is required; run: pip install -r hil/requirements.txt") from error
        self.serial = serial.Serial(port, baudrate=baudrate, timeout=0.1)
        self.serial.reset_input_buffer()

    def exchange(self, line, timeout=2.0):
        self.serial.write((line + "\n").encode("ascii"))
        self.serial.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            raw = self.serial.readline()
            if not raw:
                continue
            decoded = raw.decode("utf-8", errors="replace").strip()
            if decoded.startswith("{"):
                return decoded
        raise TimeoutError(f"no HIL response within {timeout:.1f}s for {line!r}")

    def close(self):
        if self.serial.is_open:
            self.serial.close()

    @staticmethod
    def ports():
        try:
            from serial.tools import list_ports
        except ImportError:
            return []
        return [port.device for port in list_ports.comports()]
