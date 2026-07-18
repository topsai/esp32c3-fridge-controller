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


if __name__ == "__main__":
    unittest.main()
