import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class OtaProjectConfigTest(unittest.TestCase):
    def test_local_credentials_are_ignored(self):
        result = subprocess.run(
            ["git", "check-ignore", "include/ota_config.local.h"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(0, result.returncode, result.stderr)

    def test_example_contains_all_required_settings(self):
        example = (ROOT / "include" / "ota_config.example.h").read_text(encoding="utf-8")
        for setting in (
            "FRIDGE_WIFI_SSID",
            "FRIDGE_WIFI_PASSWORD",
            "FRIDGE_OTA_HOSTNAME",
            "FRIDGE_OTA_PASSWORD",
        ):
            self.assertIn(setting, example)

    def test_platformio_has_espota_upload_environment(self):
        config = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        self.assertIn("[env:esp32-c3-ota]", config)
        self.assertIn("upload_protocol = espota", config)
        self.assertIn("--auth=$sysenv{FRIDGE_OTA_PASSWORD}", config)

    def test_wifi_provisioning_uses_fixed_nonblocking_dependency(self):
        config = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        manager = (ROOT / "src" / "wifi_provisioning_manager.cpp").read_text(encoding="utf-8")
        state = (ROOT / "include" / "wifi_provisioning_state.h").read_text(encoding="utf-8")
        self.assertIn("tzapu/WiFiManager @ 2.0.17", config)
        self.assertIn("WIFI_PORTAL_TIMEOUT_MS = 60000u", state)
        self.assertIn("setConfigPortalBlocking(false)", manager)
        self.assertNotIn("autoConnect(", manager)

    def test_ota_manager_does_not_own_wifi_connections(self):
        source = (ROOT / "src" / "ota_manager.cpp").read_text(encoding="utf-8")
        self.assertNotIn("WiFi.begin(", source)
        self.assertNotIn("WiFi.disconnect(", source)
        self.assertNotIn("FRIDGE_WIFI_SSID", source)
        self.assertNotIn("FRIDGE_WIFI_PASSWORD", source)


if __name__ == "__main__":
    unittest.main()
