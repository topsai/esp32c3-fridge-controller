import tempfile
import unittest
from pathlib import Path

from hil.runner import ScenarioRunner


class FakeClient:
    def __init__(self):
        self.commands = []

    def command(self, command, timeout=2.0):
        self.commands.append(command)
        return {"ok": True, "compressor": "off", "fan": "cooldown"}


class RunnerTest(unittest.TestCase):
    def test_assertions_and_reports(self):
        scenario = {
            "name": "cooldown",
            "steps": [{"command": "STATUS", "expect": {"compressor": "off", "fan": "cooldown"}}],
        }
        with tempfile.TemporaryDirectory() as directory:
            result = ScenarioRunner(FakeClient()).run(scenario, Path(directory))
            self.assertTrue(result["passed"])
            self.assertTrue((Path(directory) / "cooldown.json").exists())
            self.assertTrue((Path(directory) / "cooldown.html").exists())


if __name__ == "__main__":
    unittest.main()
