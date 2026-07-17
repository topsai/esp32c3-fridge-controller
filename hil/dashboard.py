"""Tk real-time dashboard for a USB-connected HIL firmware."""

import argparse
import json
import queue
import threading
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk

from hil.protocol import HilClient
from hil.runner import ScenarioRunner, load_scenario
from hil.transport import SerialTransport


class Dashboard:
    def __init__(self, root, port):
        self.root = root
        self.transport = SerialTransport(port)
        self.client = HilClient(self.transport)
        self.values = {}
        self.events = queue.Queue()
        self.scenario_running = False
        self.history = {"time": [], "ntc": [], "ds18b20": [], "setpoint": []}
        root.title(f"Fridge HIL — {port}")
        self.banner = tk.Label(root, text="OUTPUTS LOCKED", fg="white", bg="green", font=("Segoe UI", 14, "bold"))
        self.banner.pack(fill="x")
        grid = ttk.Frame(root, padding=10)
        grid.pack(fill="both", expand=True)
        fields = ["uptime_ms", "setpoint", "ntc", "ds18b20", "compressor", "fan",
                  "fan_cooldown_remaining_ms", "sensor_fault", "display",
                  "expected_relay_level", "expected_fan_level", "actual_relay_level", "actual_fan_level"]
        for row, field in enumerate(fields):
            ttk.Label(grid, text=field).grid(row=row, column=0, sticky="w")
            value = ttk.Label(grid, text="—", width=24)
            value.grid(row=row, column=1, sticky="w")
            self.values[field] = value
        controls = ttk.Frame(grid)
        controls.grid(row=0, column=2, rowspan=len(fields), padx=(20, 0), sticky="n")
        ttk.Button(controls, text="Lock outputs", command=lambda: self.send("OUTPUTS LOCK")).pack(fill="x")
        ttk.Button(controls, text="Unlock outputs", command=self.unlock).pack(fill="x")
        ttk.Button(controls, text="Wake display", command=lambda: self.send("DISPLAY WAKE")).pack(fill="x")
        self.command = ttk.Entry(controls, width=38)
        self.command.pack(fill="x", pady=(10, 0))
        ttk.Button(controls, text="Send command", command=self.send_entry).pack(fill="x")
        self.scenario = ttk.Combobox(controls, values=[str(path) for path in sorted(Path("hil/scenarios").glob("*.json"))], width=36)
        self.scenario.pack(fill="x", pady=(10, 0))
        ttk.Button(controls, text="Run scenario", command=self.run_scenario).pack(fill="x")
        self.log = tk.Text(root, height=10, width=110)
        self.log.pack(fill="both", padx=10, pady=10)
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
        from matplotlib.figure import Figure
        figure = Figure(figsize=(8, 2.5), dpi=100)
        self.axis = figure.add_subplot(111)
        self.lines = {name: self.axis.plot([], [], label=name)[0] for name in ("ntc", "ds18b20", "setpoint")}
        self.axis.set_xlabel("uptime (s)")
        self.axis.set_ylabel("°C")
        self.axis.legend(loc="upper right")
        self.chart = FigureCanvasTkAgg(figure, master=root)
        self.chart.get_tk_widget().pack(fill="both", padx=10, pady=(0, 10))
        root.protocol("WM_DELETE_WINDOW", self.close)
        self.poll()
        self.drain_events()

    def send(self, command):
        try:
            response = self.client.command(command)
            self.write_log(response)
            return response
        except Exception as error:
            self.write_log({"error": str(error)})
            return None

    def unlock(self):
        if messagebox.askyesno("Dangerous output", "Confirm low-voltage test setup and unlock real GPIO outputs?"):
            self.send("OUTPUTS UNLOCK")

    def send_entry(self):
        command = self.command.get().strip()
        if command:
            self.send(command)

    def run_scenario(self):
        path = self.scenario.get()
        if not path or self.scenario_running:
            return
        self.scenario_running = True
        self.events.put({"event": "scenario_started", "path": path})
        threading.Thread(target=self._run_scenario_worker, args=(path,), daemon=True).start()

    def _run_scenario_worker(self, path):
        try:
            self.events.put(ScenarioRunner(self.client).run(load_scenario(path), Path("hil/reports")))
        except Exception as error:
            self.events.put({"error": str(error)})
        finally:
            self.scenario_running = False

    def poll(self):
        response = None if self.scenario_running else self.send("STATUS")
        if response:
            for key, label in self.values.items():
                label.configure(text=str(response.get(key, "—")))
            unlocked = response.get("outputs_unlocked", False)
            self.banner.configure(text="OUTPUTS UNLOCKED" if unlocked else "OUTPUTS LOCKED",
                                  bg="red" if unlocked else "green")
            self.update_chart(response)
        self.root.after(1000, self.poll)

    def update_chart(self, response):
        self.history["time"].append(float(response.get("uptime_ms", 0)) / 1000.0)
        for name in ("ntc", "ds18b20", "setpoint"):
            value = response.get(name)
            self.history[name].append(float("nan") if value is None else float(value))
            self.lines[name].set_data(self.history["time"][-300:], self.history[name][-300:])
        self.axis.relim()
        self.axis.autoscale_view()
        self.chart.draw_idle()

    def drain_events(self):
        while True:
            try:
                self.write_log(self.events.get_nowait())
            except queue.Empty:
                break
        self.root.after(200, self.drain_events)

    def write_log(self, value):
        self.log.insert("end", json.dumps(value, ensure_ascii=False) + "\n")
        self.log.see("end")

    def close(self):
        try:
            self.client.command("OUTPUTS LOCK", timeout=0.5)
        except Exception:
            pass
        self.transport.close()
        self.root.destroy()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True, help="serial port, for example COM5")
    args = parser.parse_args()
    root = tk.Tk()
    Dashboard(root, args.port)
    root.mainloop()


if __name__ == "__main__":
    main()
