"""Tk real-time dashboard that remains fully visible without a serial device."""

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


def disconnected_snapshot():
    """Return an empty but structurally complete disconnected state."""
    return {
        "uptime_ms": None,
        "setpoint": None,
        "ntc": None,
        "ds18b20": None,
        "compressor": None,
        "fan": None,
        "fan_cooldown_remaining_ms": None,
        "sensor_fault": None,
        "display": None,
        "expected_relay_level": None,
        "expected_fan_level": None,
        "actual_relay_level": None,
        "actual_fan_level": None,
        "outputs_unlocked": False,
    }


class ConnectionController:
    """Own serial resources separately from widget construction."""

    def __init__(self, transport_factory=SerialTransport):
        self.transport_factory = transport_factory
        self.transport = None
        self.client = None
        self.port = None

    @property
    def connected(self):
        return self.client is not None

    def connect(self, port):
        self.disconnect()
        transport = self.transport_factory(port)
        client = HilClient(transport)
        response = client.command("PING")
        if not response.get("ok"):
            transport.close()
            raise RuntimeError("HIL PING was rejected")
        self.transport, self.client, self.port = transport, client, port
        return response

    def disconnect(self):
        if self.client is not None:
            try:
                self.client.command("OUTPUTS LOCK", timeout=0.5)
            except Exception:
                pass
        if self.transport is not None:
            self.transport.close()
        self.transport = self.client = self.port = None

    def command(self, command, timeout=2.0):
        if self.client is None:
            raise ConnectionError("disconnected: select a serial port and connect")
        return self.client.command(command, timeout=timeout)


class Dashboard:
    FIELDS = [
        "uptime_ms", "setpoint", "ntc", "ds18b20", "compressor", "fan",
        "fan_cooldown_remaining_ms", "sensor_fault", "display",
        "expected_relay_level", "expected_fan_level", "actual_relay_level", "actual_fan_level",
    ]

    def __init__(self, root, port=None):
        self.root = root
        self.connection = ConnectionController()
        self.values = {}
        self.events = queue.Queue()
        self.scenario_running = False
        self.history = {"time": [], "ntc": [], "ds18b20": [], "setpoint": []}

        root.title("Fridge HIL Dashboard")
        connection_bar = ttk.Frame(root, padding=10)
        connection_bar.pack(fill="x")
        ttk.Label(connection_bar, text="Serial port").pack(side="left")
        self.port = ttk.Combobox(connection_bar, width=18)
        self.port.pack(side="left", padx=6)
        if port:
            self.port.set(port)
        ttk.Button(connection_bar, text="Refresh", command=self.refresh_ports).pack(side="left")
        ttk.Button(connection_bar, text="Connect", command=self.connect).pack(side="left", padx=4)
        ttk.Button(connection_bar, text="Disconnect", command=self.disconnect).pack(side="left")
        self.connection_label = ttk.Label(connection_bar, text="Disconnected")
        self.connection_label.pack(side="right")

        self.banner = tk.Label(root, text="DISCONNECTED · OUTPUTS LOCKED", fg="white", bg="gray", font=("Segoe UI", 14, "bold"))
        self.banner.pack(fill="x")
        grid = ttk.Frame(root, padding=10)
        grid.pack(fill="both", expand=True)
        for row, field in enumerate(self.FIELDS):
            ttk.Label(grid, text=field).grid(row=row, column=0, sticky="w")
            value = ttk.Label(grid, text="—", width=24)
            value.grid(row=row, column=1, sticky="w")
            self.values[field] = value

        controls = ttk.Frame(grid)
        controls.grid(row=0, column=2, rowspan=len(self.FIELDS), padx=(20, 0), sticky="n")
        ttk.Button(controls, text="Lock outputs", command=lambda: self.send("OUTPUTS LOCK")).pack(fill="x")
        ttk.Button(controls, text="Unlock outputs", command=self.unlock).pack(fill="x")
        ttk.Button(controls, text="Wake display", command=lambda: self.send("DISPLAY WAKE")).pack(fill="x")
        self.command = ttk.Entry(controls, width=38)
        self.command.pack(fill="x", pady=(10, 0))
        ttk.Button(controls, text="Send command", command=self.send_entry).pack(fill="x")
        self.scenario = ttk.Combobox(controls, values=[str(path) for path in sorted(Path("hil/scenarios").glob("*.json"))], width=36)
        self.scenario.pack(fill="x", pady=(10, 0))
        ttk.Button(controls, text="Run scenario", command=self.run_scenario).pack(fill="x")

        self.log = tk.Text(root, height=8, width=110)
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
        self.refresh_ports()
        self.render_snapshot(disconnected_snapshot(), connected=False)
        self.poll()
        self.drain_events()
        if port:
            root.after(100, self.connect)

    def refresh_ports(self):
        ports = SerialTransport.ports()
        self.port.configure(values=ports)
        if not self.port.get() and ports:
            self.port.set(ports[0])

    def connect(self):
        port = self.port.get().strip()
        if not port:
            self.write_log({"status": "disconnected", "message": "No serial port selected"})
            return
        try:
            response = self.connection.connect(port)
            self.connection_label.configure(text=f"Connected: {port}")
            self.write_log(response)
        except Exception as error:
            self.connection_label.configure(text="Disconnected")
            self.write_log({"status": "disconnected", "error": str(error)})

    def disconnect(self):
        self.connection.disconnect()
        self.connection_label.configure(text="Disconnected")
        self.render_snapshot(disconnected_snapshot(), connected=False)
        self.write_log({"status": "disconnected"})

    def send(self, command):
        try:
            response = self.connection.command(command)
            self.write_log(response)
            return response
        except Exception as error:
            self.write_log({"error": str(error)})
            return None

    def unlock(self):
        if not self.connection.connected:
            self.write_log({"error": "disconnected: outputs cannot be unlocked"})
        elif messagebox.askyesno("Dangerous output", "Confirm low-voltage test setup and unlock real GPIO outputs?"):
            self.send("OUTPUTS UNLOCK")

    def send_entry(self):
        command = self.command.get().strip()
        if command:
            self.send(command)

    def run_scenario(self):
        path = self.scenario.get()
        if not self.connection.connected:
            self.write_log({"error": "disconnected: connect before running a scenario"})
            return
        if not path or self.scenario_running:
            return
        self.scenario_running = True
        self.events.put({"event": "scenario_started", "path": path})
        threading.Thread(target=self._run_scenario_worker, args=(path,), daemon=True).start()

    def _run_scenario_worker(self, path):
        try:
            self.events.put(ScenarioRunner(self.connection.client).run(load_scenario(path), Path("hil/reports")))
        except Exception as error:
            self.events.put({"error": str(error)})
        finally:
            self.scenario_running = False

    def poll(self):
        if self.connection.connected and not self.scenario_running:
            response = self.send("STATUS")
            if response:
                self.render_snapshot(response, connected=True)
        self.root.after(1000, self.poll)

    def render_snapshot(self, response, connected):
        for key, label in self.values.items():
            value = response.get(key)
            label.configure(text="—" if value is None else str(value))
        unlocked = response.get("outputs_unlocked", False)
        if not connected:
            self.banner.configure(text="DISCONNECTED · OUTPUTS LOCKED", bg="gray")
            self.clear_chart()
        else:
            self.banner.configure(text="OUTPUTS UNLOCKED" if unlocked else "OUTPUTS LOCKED",
                                  bg="red" if unlocked else "green")
            self.update_chart(response)

    def clear_chart(self):
        self.history = {"time": [], "ntc": [], "ds18b20": [], "setpoint": []}
        for line in self.lines.values():
            line.set_data([], [])
        self.axis.relim()
        self.axis.autoscale_view()
        self.chart.draw_idle()

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
        self.connection.disconnect()
        self.root.destroy()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", help="optional serial port, for example COM5")
    args = parser.parse_args()
    root = tk.Tk()
    Dashboard(root, args.port)
    root.mainloop()


if __name__ == "__main__":
    main()
