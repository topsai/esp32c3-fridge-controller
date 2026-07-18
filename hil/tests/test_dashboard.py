import unittest

from hil.dashboard import ConnectionController, disconnected_snapshot, temperature_button_command


class ConnectionControllerTest(unittest.TestCase):
    def test_does_not_open_serial_until_connect_is_requested(self):
        opened = []

        def factory(port):
            opened.append(port)
            return object()

        controller = ConnectionController(factory)
        self.assertEqual([], opened)
        self.assertFalse(controller.connected)

    def test_disconnected_snapshot_contains_fields_without_fake_measurements(self):
        snapshot = disconnected_snapshot()
        expected = {
            "uptime_ms", "setpoint", "ntc", "ds18b20", "compressor", "fan",
            "fan_cooldown_remaining_ms", "sensor_fault", "display",
            "expected_relay_level", "expected_fan_level", "actual_relay_level",
            "actual_fan_level", "outputs_unlocked",
        }
        self.assertTrue(expected.issubset(snapshot))
        self.assertIsNone(snapshot["ntc"])
        self.assertIsNone(snapshot["ds18b20"])
        self.assertIsNone(snapshot["compressor"])
        self.assertFalse(snapshot["outputs_unlocked"])

    def test_temperature_buttons_use_physical_button_commands(self):
        self.assertEqual("BUTTON DOWN CLICK", temperature_button_command(-1))
        self.assertEqual("BUTTON UP CLICK", temperature_button_command(1))
        with self.assertRaises(ValueError):
            temperature_button_command(0)


if __name__ == "__main__":
    unittest.main()
