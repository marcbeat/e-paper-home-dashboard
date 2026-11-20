# AGENTS.md

PlatformIO project: ESP32 + Arduino framework, default env `esp32dev` (espressif32).
Entry point is `Station.ino` (repo root, `src_dir = "."`); helpers live in `src/`:
`common.h` (OWM weather fetch/decode), `lang.h`, `lang_gr.h` (UI strings),
`epaper_fonts.h`, `config.h` (copied from `config.example.h`).

## Build & verify
- Build/upload with the PlatformIO IDE. Last used: `pio run` (see `build.log`).
  `pio` is **not on PATH** in this shell.
- `pio run -t upload` uploads. `upload_port = COM6` is hardcoded in `platformio.ini`:
  uploads fail if COM6 is not connected (see `upload.log`).
- `pio device monitor` — serial monitor, `monitor_speed = 115200`.
- No tests or lint/typecheck targets.
- Libs (last build): GxEPD2 1.6.9, ArduinoJson 7.4.3, U8g2_for_Adafruit_GFX 1.8.0,
  Adafruit GFX 1.12.6, Adafruit BusIO 1.17.4.

## Display
- Waveshare 12.48" e-Paper (1304x984, B/W) on the Waveshare ESP32 driver board.
- 4-controller panel (M1/S1/M2/S2) with separate SPI pins, CS, RST and BUSY lines
  (GPIO 13/12/14/23/22/16/19, DC1/DC2, RST1/RST2, BUSY M1/S1/M2/S2 in `Station.ino`).
- GPIO 5 is RST2 of the panel — Station.ino `#undef`s BUILTIN_LED because of it.

## Config & credentials
- All settings live in `src/config.h` (git-ignored; template in `src/config.example.h`): SSID/password, OWM `apikey`,
  LAT/LON, City, Language, Units (`M`/`I`), Hemisphere, Timezone, NTP server.
- OWM 2.5 API (`/data/2.5/weather` + `/data/2.5/forecast`, `src/common.h`);
  keep under ~60 reads/min per OWM.
- Version string `String version` at top of `Station.ino` (e.g. "1.3 / 12.48in");
  bump it when display layout changes (changelog at bottom of `Station.ino`).
- Deep-sleep tuning: `SleepDuration`/`WakeupTime`/`SleepTime` in `Station.ino`.

## Known symptom
- `serial.log` routinely records "*** Failed to Rx Weather data ***" on WAN
  connectivity issues — not necessarily a bug.

## Version control (see `.agents/docs/VERSION-CONTROL.md`)
All agents need to obey by the rules and guidelines defined in `.agents/docs/VERSION-CONTROL.md`.