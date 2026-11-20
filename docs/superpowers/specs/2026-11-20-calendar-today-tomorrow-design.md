# Calendar Today & Tomorrow View Design

## Intent & Why
Replace the single-day `"Heutige Termine"` view with a combined **Today + Tomorrow** view inside each calendar card (`Piet` and `Nati`). Throughout the day, users can see any remaining/ongoing events for today alongside tomorrow's upcoming schedule at a glance, without waiting for an evening cutoff.

## Goals, Non-Goals, and Anti-Goals

### Goals
- Update the Google Apps Script Web App (`README.md`) to accept a `datetime` query parameter (the ESP32's current local ISO-8601 timestamp) and return events from that point in time through `23:59:59` the following day, including all-day events for both days.
- Return separate `today` and `tomorrow` arrays per calendar (`{ cal1: { today, tomorrow }, cal2: { today, tomorrow } }`).
- Render both days stacked inside each card (`Piet` / `Nati`) under `"Heute"` and `"Morgen"` (`"Today"` / `"Tomorrow"`) labels, keeping existing row truncation (`TruncateTextToWidth`) and `"+N weitere"` overflow behavior.
- Update the outer section heading from `"Heutige Termine"` / `"Today's Events"` to `"Termine"` / `"Events"`.
- Bump `version` in `Station.ino` and append a changelog entry.

### Non-Goals
- No lookahead beyond tomorrow, no event end times/locations, and no changes to the `635×290` bottom-left section geometry.
- No automated unit tests (repository has no test runner; verification is a PlatformIO build plus Apps Script test call).

### Anti-Goals
- Do not drop today's all-day events when `datetime` is past `00:00`.
- Do not show `"Morgen"` or tomorrow's events when today overflows (`+N weitere` is displayed) or when zero event rows remain beneath `"Morgen"`.

## Constraints
- **Platform:** ESP32 (`esp32dev`), `ArduinoJson` 7.4.3, `HTTPClient`, `WiFiClientSecure::setInsecure()` (10s timeout).
- **Time source:** `Station.ino` already syncs local wall-clock time via NTP in `SetupTime()` / `UpdateLocalTime()`.
- **Card budget:** Each card (`311×260` below the `30px` section title) has a `36px` header and `224px` body (`maxRows = 8` at `rowStep = 26`).
- **External deployment:** The Google Apps Script lives on `script.google.com` and must be manually updated and redeployed alongside flashing the ESP32 firmware.

## Approaches Considered
1. **Nested `today` / `tomorrow` arrays per calendar (Chosen):** Apps Script accepts `datetime` and returns `{ cal1: { today: [...], tomorrow: [...] }, cal2: { today: [...], tomorrow: [...] } }`. Keeps day boundaries and multi-day all-day event duplication on the server and gives each day an independent event buffer on the ESP32.
2. **Flat `cal1` / `cal2` arrays with a `day` tag:** Mixes both days into one array cap before splitting and complicates multi-day events.
3. **Two HTTP requests (`today` and `tomorrow`):** Doubles slow TLS redirects to `script.google.com` and increases battery drain.

## Design Details

### 1. Google Apps Script Bridge (`README.md`)
- `doGet(e)` reads `e.parameter.datetime` (falling back to `new Date()` if absent) and computes:
  - `from`: parsed timestamp of `datetime`
  - `todayStart` / `todayEnd`: `00:00:00` – `23:59:59` of `datetime`'s local date
  - `tomorrowStart` / `tomorrowEnd`: `00:00:00` – `23:59:59` of the following day
- Queries `cal.getEvents(from, tomorrowEnd)` once per configured calendar:
  - **Today list:** events overlapping `[from, todayEnd]` (captures today's all-day events, currently ongoing timed events, and upcoming timed events today).
  - **Tomorrow list:** events overlapping `[tomorrowStart, tomorrowEnd]` (captures tomorrow's all-day events, multi-day all-day events spanning both days, and all timed events tomorrow).
- Each item retains `{ time, title, allDay }`.

### 2. ESP32 Fetch & Data Model (`src/calendar.h`, `Station.ino`)
- Store `today` and `tomorrow` event arrays + counts for `cal1` and `cal2` (each bounded by `MAX_CALENDAR_EVENTS = 10`).
- Rename `ReceiveTodayCalendars(bool print)` to `ReceiveCalendars(bool print)`.
- Format local NTP time via `getLocalTime(&timeinfo)` as ISO-8601 (`YYYY-MM-DDTHH:MM:SS+HH:MM`) and append `&datetime=...` to the bridge URL.
- Update `DecodeCalendarJson()` to read `doc["cal1"]["today"]`, `doc["cal1"]["tomorrow"]`, `doc["cal2"]["today"]`, and `doc["cal2"]["tomorrow"]`.

### 3. Localization (`src/lang_gr.h`, `src/lang.h`)
- Replace `TXT_TODAY_EVENTS` with `TXT_CAL_EVENTS`: `"Termine"` (`lang_gr.h`) / `"Events"` (`lang.h`).
- Add `TXT_TODAY` (`"Heute"` / `"Today"`) and `TXT_TOMORROW` (`"Morgen"` / `"Tomorrow"`).

### 4. Card Layout & Overflow Rules (`Station.ino`)
Inside `DrawCalendarListCard(...)` (`maxRows` rows total):
1. If `!configured`, render `TXT_CAL_NOT_CONF` and return.
2. Always render the `"Heute"` subheader first.
3. **Today section:**
   - If `todayCount == 0`: render `TXT_NO_EVENTS` (`"Keine Termine"`), consuming 1 row below `"Heute"`.
   - Else if `todayCount > (maxRows - 1)`: render `maxRows - 2` today events followed by `"+N weitere"` and **return** (`"Morgen"` and tomorrow's events are omitted completely).
   - Else: render all `todayCount` events.
4. **Tomorrow section:**
   - Let `availTomorrow = remainingRows - 1` (reserving 1 row for the `"Morgen"` subheader).
   - If `availTomorrow <= 0`: return without rendering `"Morgen"`.
   - Render `"Morgen"` subheader.
   - If `tomorrowCount == 0`: render `TXT_NO_EVENTS`.
   - Else if `tomorrowCount > availTomorrow`: render `availTomorrow - 1` tomorrow events followed by `"+N weitere"`.
   - Else: render all `tomorrowCount` tomorrow events.

## Builder Decisions Left Open
- Visual styling of the `"Heute"` / `"Morgen"` subheaders (e.g., `helvB12_tf` with a short divider or subtle underline vs plain label within `rowStep = 26`).
- Struct grouping for `cal1`/`cal2` today + tomorrow buffers in `src/calendar.h` and `Station.ino`.
