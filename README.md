# ESP32 12.48" E-Paper Weather Station

Weather station firmware for an ESP32 driving a **Waveshare 12.48" e-Paper (1304×984, B/W)** display (`GDEW1248T3` / `GxEPD2_1248`). Fetches current weather and 3-hour forecasts from the OpenWeatherMap 2.5 API, renders conditions, wind compass, astronomy (sunrise/sunset, moon phase), 3-hour forecast tiles, and trend graphs (pressure, temperature, humidity, precipitation), then enters deep sleep.

Based on David Bird's (G6EJD) ESP32 e-Paper Weather Display, adapted for the 4-controller 12.48" Waveshare panel.

---

## Hardware

- **MCU:** Waveshare ESP32 Driver Board (`esp32dev`)
- **Display:** Waveshare 12.48" e-Paper Module (1304×984, Black/White, 4 controllers: `M1`, `S1`, `M2`, `S2`)
- **Power:** Stable 5V supply required (high peak current during full panel refresh) and 3.3V logic level

### Pin Mapping ([Station.ino](Station.ino#L58-L72))

| Signal | GPIO | Notes |
| :--- | :--- | :--- |
| `SCK` | 13 | SPI Clock |
| `MISO` | 12 | Passed to constructor (unused by panel) |
| `MOSI` | 14 | SPI Data |
| `CS_M1` / `CS_S1` | 23 / 22 | Chip Select Master 1 / Slave 1 |
| `CS_M2` / `CS_S2` | 16 / 19 | Chip Select Master 2 / Slave 2 |
| `DC1` / `DC2` | 25 / 17 | Data/Command |
| `RST1` / `RST2` | 33 / 5 | Reset (`BUILTIN_LED` is undefined because GPIO 5 is `RST2`) |
| `BUSY_M1` / `BUSY_S1` | 32 / 26 | Busy signals M1 / S1 |
| `BUSY_M2` / `BUSY_S2` | 18 / 4 | Busy signals M2 / S2 |

---

## Quick Start

1. **Install the Windows USB Driver:**
   - Install the [Silicon Labs CP210x USB to UART Bridge VCP Driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) (`VID_10C4&PID_EA60`, enumerates as `COM6` by default; newer Waveshare board revisions may use the [WCH CH343 driver](https://www.wch-ic.com/downloads/CH343SER_EXE.html)).
2. **Create your config file** from the template:
   ```powershell
   Copy-Item src/config.example.h src/config.h
   ```
3. **Edit `src/config.h`** with your credentials and location:
   - `ssid` / `password`: 2.4 GHz Wi-Fi credentials
   - `apikey`: Free OpenWeatherMap API key (`https://openweathermap.org/`)
   - `LAT` / `LON`, `City`, `Country`: Target location
   - `Units`: `"M"` (Metric) or `"I"` (Imperial)
   - `Timezone` & `ntpServer`: POSIX timezone string and NTP pool
4. **Build and flash** via PlatformIO ([platformio.ini](platformio.ini)):
   - **Hold down the `BOOT` button** on the ESP32 board before/while starting the upload (required for new flashes or when the ESP32 is in deep sleep), and release it once `esptool` connects and begins writing.
   ```powershell
   pio run
   pio run -t upload
   ```
   > **Note:** `upload_port` is set to `COM6` in `platformio.ini`. Update it if your ESP32 enumerates on a different port.
5. **Monitor serial output** (`115200` baud):
   ```powershell
   pio device monitor
   ```

---

## Google Calender Integration

Prerequisite: Google Apps Script Deployment
Ensure cal2's Google Calendar is shared (at least "See all event details") with the Google account hosting the script.
Open script.google.com -> New project, paste:

```javascript
const SHARED_TOKEN = "REPLACE_WITH_LONG_RANDOM_TOKEN";

function formatEvents(calId, startOfDay, endOfDay) {
  if (!calId) return [];
  const cal = CalendarApp.getCalendarById(calId);
  if (!cal) return [];
  return cal.getEvents(startOfDay, endOfDay).map((ev) => {
    const allDay = ev.isAllDayEvent();
    const start = ev.getStartTime();
    const hh = String(start.getHours()).padStart(2, "0");
    const mm = String(start.getMinutes()).padStart(2, "0");
    return {
      time: allDay ? "" : `${hh}:${mm}`,
      title: ev.getTitle() || "(Ohne Titel)",
      allDay: allDay,
    };
  });
}

function doGet(e) {
  if (!e || e.parameter.token !== SHARED_TOKEN) {
    return ContentService.createTextOutput(JSON.stringify({ error: "unauthorized" }))
      .setMimeType(ContentService.MimeType.JSON);
  }
  const now = new Date();
  const startOfDay = new Date(now.getFullYear(), now.getMonth(), now.getDate(), 0, 0, 0);
  const endOfDay   = new Date(now.getFullYear(), now.getMonth(), now.getDate(), 23, 59, 59);
  const payload = {
    cal1: formatEvents(e.parameter.cal1 || "", startOfDay, endOfDay),
    cal2: formatEvents(e.parameter.cal2 || "", startOfDay, endOfDay),
  };
  return ContentService.createTextOutput(JSON.stringify(payload))
    .setMimeType(ContentService.MimeType.JSON);
}
```

Click `Deploy` -> `New deployment` -> `Web app`:
Execute as: `Me`
Who has access: `Anyone` (gated by SHARED_TOKEN)
Copy the /exec URL into `calendarBridgeUrl`, SHARED_TOKEN into `calendarBridgeToken`, and both Calendar IDs into `calendarId1` / `calendarId2` in src/config.h.



---

## Customization

- **UI Language:** In [Station.ino](Station.ino#L41-L47), switch between `#include "lang_gr.h"` (German, default) and `#include "lang.h"` (English).
- **Sleep Schedule:** Configured in [Station.ino](Station.ino#L129-L131):
  - `SleepDuration = 30` (updates every 30 minutes, aligned to `:00` and `:30`)
  - `WakeupTime = 7` / `SleepTime = 23` (active hours `07:00`–`23:59`; deep-sleeps overnight)
- **On-Screen Event Log:** Set `EventThreshold = 1` in [Station.ino](Station.ino#L104) to render all boot/Wi-Fi diagnostic events directly on the e-Paper screen.

---

## Project Structure

- [Station.ino](Station.ino) — Display initialization, layout rendering, Wi-Fi/NTP sync, and deep-sleep management
- [src/common.h](src/common.h) — OpenWeatherMap `/data/2.5/weather` and `/data/2.5/forecast` HTTP fetch & JSON decoding
- [src/config.example.h](src/config.example.h) — Configuration template (`src/config.h` is git-ignored)
- [src/lang.h](src/lang.h) / [src/lang_gr.h](src/lang_gr.h) — English and German UI translations
- [src/epaper_fonts.h](src/epaper_fonts.h) — Custom font definitions
- [platformio.ini](platformio.ini) — PlatformIO environment (`esp32dev`) and library dependencies (`GxEPD2`, `ArduinoJson`, `U8g2_for_Adafruit_GFX`, `Adafruit GFX`)
