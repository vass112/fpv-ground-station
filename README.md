# FPV Ground Station

Emergency-response ground station for an **F450 quadcopter DIY build** — live ESP32-CAM video + APM 2.8 telemetry on a single offline HTML page, with a DIY ESP-NOW RC link.

**Flight stack:** APM 2.8 + M8N GPS · **Camera:** ESP32-CAM (AI-Thinker) · **RC link:** [esp-rc-link](https://github.com/adithya-in/esp-rc-link) (ESP32-C3 TX → ESP-NOW → ESP8266 RX → PPM into APM Input 1, Input 2↔3 jumper on)
**Camera firmware:** [esp-rc-link `esp32cam_livefeed`](https://github.com/adithya-in/esp-rc-link/tree/main/firmware/esp32cam_livefeed) — stock CameraWebServer adapted to AP mode.

## Screens

| Component | File | What it is |
|---|---|---|
| Ground station | [`ground-station.html`](ground-station.html) | Self-contained, offline. Program monitor + transport (Arm → Rec → Snapshot → Mark), camera sheet, track sheet (Link/Rec/GPS/Batt/Mode), cue-sheet log, alarms with beeps, evidence burn-in, night mode |
| Telemetry bridge | [`bridge.py`](bridge.py) | pymavlink: APM 2.8 on COM port @115200 → `GET http://127.0.0.1:5000/api/telemetry` serving `{fix,sats,batt,mode,alt,spd,lat,lon,age}` |
| Camera firmware | [`cam-firmware/`](cam-firmware/) | PlatformIO project + Arduino sketch variant for the ESP32-CAM node we authored first; the drone currently flies with the repo-linked livefeed firmware (hotspot `ESP32-CAM` / `12345678`) |
| Screenshots | `.impeccable/review/` | Last verified desktop / mobile renders |
| Design system | [`DESIGN.md`](DESIGN.md) | FIELD RECORDER direction — tokens, contracts, documented exemptions |

## Camera ↔ webapp protocol (esp-rc-link firmware)

The page speaks the firmware's exact contract:

- **Stream (MJPEG):** `http://192.168.4.1:81/stream` → `<img>`, polled for `naturalWidth` as link-up proof
- **Snapshot/Record:** browser-side canvas capture works — this firmware sends `Access-Control-Allow-Origin: *` on all endpoints (stock does not)
- **Status sync:** on connect, `GET /status` (CORS open) pulls the camera's real framesize + torch state so the controls never lie
- **Camera sheet** sends `GET /control?var=<var>&val=<n>` (`mode:"no-cors"`):
  - `framesize` — true enum values verified by decoding the firmware's shipped UI gzip: `QVGA=6, CIF=8, HVGA=9, VGA=10, SVGA=11, XGA=12, HD=13, SXGA=14, UXGA=15`
  - `brightness` (−2…2), `aec_value` (0–1200)
  - `led_intensity` (0–255 PWM) → AI-Thinker flash LED as a dimmable torch
- Firmware boots at **QVGA** regardless of PSRAM

## Run it

**Ground station:**
```
start ground-station.html        # or open in any browser — no server needed
```
Join the CAM's hotspot (`ESP32-CAM`, key `12345678`) → press **Connect**.

**Telemetry (real data replaces the amber DEMO values):**
```
pip install pymavlink
python bridge.py                 # APM on the configured COM port @ 115200
```
Page treats telemetry `age > 3 s` as link-down and falls back to labelled demo values.

**Camera firmware** (if you want our variant instead of livefeed):
```
cd cam-firmware && pio run -t upload --upload-port COM14
```
AI-Thinker pin map; flash mode = GPIO0→GND + power-cycle; TXD↔U0R / RXD↔U0T crossover; 5 V supply required.

## Field wiring (RC link + power)

- PPM: D1 mini `D2/GPIO4` → APM **Input 1** signal; **jumper Input 2↔3** signal pins (required for PPM sum); common GND. 3.3 V PPM usually decodes; buffer to 5 V if channels read 0.
- Power module feeds APM only (5.3 V); tape off 3 of 4 ESC BEC red wires; M8N on GPS port + I2C compass lead; MAG jumper stays ON until external compass is proven.
- CAM power: stable 5 V, ~300–500 mA peaks — USB-TTL rails brown out under load.

## Notes

- Stock CameraWebServer UI lacks CORS; our target firmware enables it, but the page self-probes capture capability and falls back gracefully (VLC workaround) if a camera blocks the canvas.
- No secrets in repo — hotspot keys are the firmware's non-sensitive defaults.
- MJPEG over Wi-Fi adds ~100–300 ms latency; fine for framing/recording, not low-latency manual flying.
