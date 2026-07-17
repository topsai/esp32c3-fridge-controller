"""JSON scenario execution and JSON/HTML report generation."""

import html
import json
import time
from datetime import datetime, timezone
from pathlib import Path


class ScenarioRunner:
    def __init__(self, client):
        self.client = client

    def run(self, scenario, report_directory):
        started = datetime.now(timezone.utc).isoformat()
        steps = []
        passed = True
        for index, definition in enumerate(scenario["steps"], start=1):
            step_started = time.monotonic()
            try:
                if "wait_seconds" in definition:
                    self._wait_with_heartbeat(float(definition["wait_seconds"]), float(definition.get("heartbeat_seconds", 2.0)))
                    response = {"ok": True}
                else:
                    response = self.client.command(definition["command"], timeout=float(definition.get("timeout", 2.0)))
                failures = [f"{key}: expected {expected!r}, got {response.get(key)!r}"
                            for key, expected in definition.get("expect", {}).items()
                            if response.get(key) != expected]
                step_passed = not failures
            except Exception as error:
                response, failures, step_passed = None, [str(error)], False
            passed = passed and step_passed
            steps.append({"index": index, "definition": definition, "response": response,
                          "passed": step_passed, "failures": failures,
                          "duration_seconds": round(time.monotonic() - step_started, 3)})
        result = {"scenario": scenario["name"], "started_utc": started, "passed": passed, "steps": steps}
        report_directory = Path(report_directory)
        report_directory.mkdir(parents=True, exist_ok=True)
        stem = scenario["name"].replace(" ", "_").lower()
        (report_directory / f"{stem}.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        rows = "".join(f"<tr><td>{step['index']}</td><td>{html.escape(str(step['definition']))}</td>"
                       f"<td>{'PASS' if step['passed'] else 'FAIL'}</td><td>{html.escape('; '.join(step['failures']))}</td></tr>"
                       for step in steps)
        page = f"<!doctype html><meta charset='utf-8'><title>{html.escape(scenario['name'])}</title>"
        f"<h1>{html.escape(scenario['name'])}: {'PASS' if passed else 'FAIL'}</h1>"
        f"<table border='1'><tr><th>#</th><th>Step</th><th>Result</th><th>Details</th></tr>{rows}</table>"
        (report_directory / f"{stem}.html").write_text(page, encoding="utf-8")
        return result

    def _wait_with_heartbeat(self, seconds, interval):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.client.command("PING")
            time.sleep(min(interval, max(0.0, deadline - time.monotonic())))


def load_scenario(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))
