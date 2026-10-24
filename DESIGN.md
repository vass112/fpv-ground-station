# DESIGN.md — FPV-1 Ground Station

Field-recorder instrument world (seed a321afcf, assigned direction 6 of 7, code-led single file).

## Scene
Builder at bench, then open field. Laptop or phone, daylight glare, one eye on the quad. Dark ground picked from the scene: backlit-instrument legibility, not category habit. Fully offline — no CDN, system stacks only — because the field has no internet.

## Tokens
- Grounds: `--ground #0b0d0e`, `--panel #121516`, `--panel-2 #171b1d`, hairline `--line #262c2e`.
- Ink: `--ink #e8ecec`, `--ink-dim #9aa3a3`, `--ink-faint #8b9393` (raised from #5c6464 to clear 4.5:1 — detector-verified).
- Signal colors with reserved jobs: `--rec #ff3b30` armed/recording only, `--lock #2ecc71` locked/healthy only, `--warn #ffb340` degraded/demo only. Neutrals everywhere else.
- Focus/selection use `--focus #7dd3fc` and amber `::selection`, both themed from the world.

## Type
- Data: `ui-monospace` stack, always `tabular-nums`, fixed-width value slots so tiles never reflow when numbers change (folio-register raise).
- Labels: system sans, 800 weight, tracked uppercase at small sizes. No eyebrow kickers; headings carry their own weight.

## Composition
- Program monitor large left with tally-bar (red top edge while recording) and an OSD margin strip (alt/spd/sat/bat/mode) — telemetry never covers program center (orienteering-truth raise).
- Right track sheet: LINK / REC / GPS / BATT / MODE rows, state lamp + tabular value each.
- Transport bar: session timecode, ARM → REC two-step (track-arming semantics: no lost footage to mis-taps), snapshot.
- OV3660 exposure controls live in a slide-aside sheet, not the instrument face.
- Phone ≤900px: stream owns the viewport, action glyph rail beneath, sticky transport (vertical-feed raise).

## States
No-signal / tuning / unreachable-with-recovery / live; armed / recording with elapsed; DEMO (amber) vs LIVE (green) telemetry badge; capture-blocked (camera CORS) disables snap/record with VLC fallback named. Errors always name the problem and the recovery.

## Exemptions (committed, not slop)
- Lamp `box-shadow` bloom (flagged `dark-glow` by the detector): a lit indicator must read as lit at 3 meters in sunlight. The glow IS the on-state, applied only to lamps and the recording tally — never to text, borders, or panels.

## Contracts
- Video in: MJPEG `http://192.168.4.1:81/stream` (CAM in AP mode, stock CameraWebServer).
- Camera out: `GET {cam}/control?var=framesize|brightness|aec_value|flash&val=n`.
- Telemetry in: `GET http://127.0.0.1:5000/api/telemetry` → `{fix,sats,batt,mode,alt,spd}`; absent bridge = labeled DEMO simulation. MAVLink→HTTP bridge is the defined next build.
## Emergency-response extension (round 2 — world unchanged)
- Track sheet GPS row now carries live position (`lat,lon` 6dp) + COPY button for relaying coords to responders; OSD margin gained a POS field.
- MARK button: timestamped incident marker in the cue sheet with coords, auto-grabs a still when the capture path is open, notes `[ON CLIP]` when recording.
- Alarm banner + throttled WebAudio beeps: battery-critical (red, LAND NOW), GPS-fix-lost and video-link-down-while-armed (amber). Sticky video alarm clears only on reconnect.
- Night mode: full red-on-black token swap + dimmed program (preserves night vision).
- Evidence burn-in: every recorded frame and snapshot carries timestamp + coords + mode in a bottom strip.
- Telemetry contract extended with `lat,lon,age`; page treats bridge data older than 3s as link-down and falls back to labeled DEMO.
- Companion: `bridge.py` (pymavlink, COM13 @115200 → `GET 127.0.0.1:5000/api/telemetry`, CORS-open). Run with `python bridge.py`; leave it running while the page is open.
- Snapshots save `FPV1_<timestamp>.jpg`, clips `FPV1_<timestamp>.webm`, cue-sheet log kept in-page (60 entries).
