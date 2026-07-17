from pathlib import Path
import unittest


SOURCE = Path(__file__).parents[1] / "src" / "main.cpp"


class ScreenTimeoutRegressionTest(unittest.TestCase):
    def test_timeout_uses_time_sampled_after_button_callbacks(self):
        source = SOURCE.read_text(encoding="utf-8")

        self.assertIn(
            "if (millis() - lastInteraction >= SCREEN_TIMEOUT)",
            source,
            "OLED timeout must sample millis() after button callbacks update lastInteraction",
        )
        self.assertNotIn(
            "if (now - lastInteraction >= SCREEN_TIMEOUT)",
            source,
            "a loop-start timestamp can precede lastInteraction and underflow unsigned subtraction",
        )


if __name__ == "__main__":
    unittest.main()
