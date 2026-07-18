from pathlib import Path
import unittest


SOURCE = (Path(__file__).parents[1] / "src" / "main.cpp").read_text(encoding="utf-8")


class WifiProvisioningIntegrationTest(unittest.TestCase):
    def test_dual_button_gesture_precedes_onebutton_callbacks(self):
        sample = "const bool bothButtonsPressed = digitalRead(SWA) == LOW && digitalRead(SWB) == LOW;"
        gesture = (
            "wifiProvisioningManager.pollButtons(" 
            "bothButtonsPressed, otaManager.isUpdating(), now)"
        )
        guard = "if (!consumeTemperatureButtons)"
        self.assertIn(sample, SOURCE)
        self.assertIn(gesture, SOURCE)
        self.assertIn(guard, SOURCE)
        self.assertLess(SOURCE.index(sample), SOURCE.index("button1.tick();"))
        self.assertLess(SOURCE.index(gesture), SOURCE.index("button1.tick();"))

    def test_wifi_manager_is_polled_without_replacing_control_loop(self):
        self.assertIn("wifiProvisioningManager.poll(now);", SOURCE)
        self.assertIn("updateFanLogic(now);", SOURCE)
        self.assertIn("DS18B20Read(now);", SOURCE)


if __name__ == "__main__":
    unittest.main()
