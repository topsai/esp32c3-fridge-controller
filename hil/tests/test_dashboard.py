import unittest

from hil.dashboard import ConnectionController, offline_snapshot


class ConnectionControllerTest(unittest.TestCase):
    def test_does_not_open_serial_until_connect_is_requested(self):
        opened = []

        def factory(port):
            opened.append(port)
            return object()

        controller = ConnectionController(factory)
        self.assertEqual([], opened)
        self.assertFalse(controller.connected)

    def test_offline_snapshot_contains_every_dashboard_field(self):
        snapshot = offline_snapshot()
        expected = {
            "uptime_ms", "setpoint", "ntc", "ds18b20", "compressor", "fan",
            "fan_cooldown_remaining_ms", "sensor_fault", "display",
            "expected_relay_level", "expected_fan_level", "actual_relay_level",
            "actual_fan_level", "outputs_unlocked",
        }
        self.assertTrue(expected.issubset(snapshot))


if __name__ == "__main__":
    unittest.main()
