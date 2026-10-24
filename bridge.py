"""FPV-1 telemetry bridge: APM (MAVLink, COM13) -> HTTP for the ground station.

Reads HEARTBEAT / GPS_RAW_INT / GLOBAL_POSITION_INT / SYS_STATUS and serves
GET http://127.0.0.1:5000/api/telemetry ->
  {fix,sats,batt,mode,alt,spd,lat,lon,age}
`age` seconds since the last MAVLink message; the page treats age > 3 as link-down.
"""
import json
import time
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT, BAUD = "COM13", 115200

state = {
    "fix": False, "sats": 0, "batt": 0.0, "mode": "--",
    "alt": 0.0, "spd": 0.0, "lat": None, "lon": None, "age": 999.0,
}
_last_msg = 0.0


_link = None


def open_link():
    global _link
    from pymavlink import mavutil
    # NOTE: opening asserts DTR, which reboots the APM; the heartbeat
    # wait below covers the reboot. Any failure -> caller retries.
    m = mavutil.mavlink_connection(PORT, baud=BAUD)
    m.wait_heartbeat(timeout=20)
    print("bridge: heartbeat from sysid %d" % m.target_system, flush=True)
    _link = m
    return m


def mav_loop():
    global _last_msg
    while True:
        try:
            m = open_link()
        except Exception as e:
            print("bridge: link error: %s" % e, flush=True)
            time.sleep(4)
            continue
        try:
            while True:
                msg = m.recv_match(blocking=True, timeout=5)
                if msg is None:
                    continue
                _last_msg = time.time()
                handle(msg)
        except Exception as e:
            print("bridge: stream error, reconnecting: %s" % e, flush=True)
        try:
            m.close()
        except Exception:
            pass
        time.sleep(4)  # let USB re-enumerate after a reboot


def handle(msg):
    t = msg.get_type()
    try:
        if t == "HEARTBEAT":
            if _link is not None:
                state["mode"] = _link.flightmode
        elif t == "GPS_RAW_INT":
            state["fix"] = msg.fix_type >= 3
            state["sats"] = msg.satellites_visible
        elif t == "GLOBAL_POSITION_INT":
            state["lat"] = msg.lat / 1e7
            state["lon"] = msg.lon / 1e7
            state["alt"] = msg.relative_alt / 1000.0
            state["spd"] = (msg.vx ** 2 + msg.vy ** 2) ** 0.5 / 100.0
        elif t == "SYS_STATUS":
            state["batt"] = msg.voltage_battery / 1000.0
    except Exception:
        pass


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != "/api/telemetry":
            self.send_response(404)
            self.end_headers()
            return
        payload = dict(state)
        payload["age"] = round(time.time() - _last_msg, 1) if _last_msg else 999.0
        body = json.dumps(payload).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *a):
        pass


if __name__ == "__main__":
    import os
    import tempfile
    pidfile = os.path.join(tempfile.gettempdir(), "opencode", "fpv-bridge.pid")
    os.makedirs(os.path.dirname(pidfile), exist_ok=True)
    try:
        with open(pidfile) as f:
            old = int(f.read().strip())
        os.kill(old, 0)  # alive?
        print("bridge: already running as PID %d — refusing second instance" % old, flush=True)
        raise SystemExit(1)
    except (ValueError, OSError, FileNotFoundError):
        pass
    with open(pidfile, "w") as f:
        f.write(str(os.getpid()))
    threading.Thread(target=mav_loop, daemon=True).start()
    print("bridge: serving http://127.0.0.1:5000/api/telemetry", flush=True)
    HTTPServer(("127.0.0.1", 5000), Handler).serve_forever()
