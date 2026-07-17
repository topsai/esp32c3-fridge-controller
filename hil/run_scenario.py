"""Command-line scenario runner."""

import argparse
from pathlib import Path

from hil.protocol import HilClient
from hil.runner import ScenarioRunner, load_scenario
from hil.transport import SerialTransport


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("scenario")
    parser.add_argument("--reports", default="hil/reports")
    args = parser.parse_args()
    transport = SerialTransport(args.port)
    try:
        result = ScenarioRunner(HilClient(transport)).run(load_scenario(args.scenario), Path(args.reports))
        print("PASS" if result["passed"] else "FAIL")
        raise SystemExit(0 if result["passed"] else 1)
    finally:
        try:
            HilClient(transport).command("OUTPUTS LOCK", timeout=0.5)
        except Exception:
            pass
        transport.close()


if __name__ == "__main__":
    main()
