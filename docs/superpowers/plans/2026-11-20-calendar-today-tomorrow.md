# Calendar Today & Tomorrow View Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Display both today's remaining/ongoing events and tomorrow's events stacked under `"Heute"` / `"Morgen"` labels inside each calendar card (`Piet` / `Nati`), powered by a `datetime`-aware Google Apps Script update.

**Architecture:** The ESP32 formats its local NTP timestamp as ISO-8601 (`YYYY-MM-DDTHH:MM:SS+HH:MM`) and passes `&datetime=` to the Google Apps Script Web App. The Apps Script fetches `[from, tomorrowEnd]` once per calendar and returns `{ cal1: { today, tomorrow }, cal2: { today, tomorrow } }`, which `src/calendar.h` decodes into `CalendarData` structs and `Station.ino` renders with per-day subheaders and overflow suppression.

**Tech Stack:** ESP32 (`esp32dev`, Arduino), `WiFiClientSecure`, `HTTPClient`, `ArduinoJson` 7.4.3, `GxEPD2` 1.6.9, `U8g2_for_Adafruit_GFX` 1.8.0, Google Apps Script (JavaScript).

**Spec:** [docs/superpowers/specs/2026-11-20-calendar-today-tomorrow-design.md](file:///Users/mb/repos/eink-station/docs/superpowers/specs/2026-11-20-calendar-today-tomorrow-design.md)

## Global Constraints

- PlatformIO target: `esp32dev` (`espressif32`, Arduino framework); build command: `/Users/mb/.local/bin/pio run` (must exit 0 with `SUCCESS`).
- Branch & commits: work on `feature/calendar-today-tomorrow` with Conventional Commits (`50/72` rule); never modify or commit `src/config.h` or the existing unstaged `platformio.ini` change.
- Card layout budget: bottom-left section `(5, 674, 635, 290)` with `cardTopOffset = 30`, `headerH = 36`, `rowStep = 26`, `maxRows = 8`.
- Version bump: update `String version` in `Station.ino` from `"1.13"` to `"1.14"` and append a changelog entry at the bottom of `Station.ino`.

## Review Focus

- **Today's all-day events after midnight:** When `datetime` is e.g. `2026-11-20T22:29:36+01:00`, today's all-day event (`00:00`–`00:00+1d`) must still match `today` because `evEnd > from`.
- **No midnight bleed of 1-day all-day events:** A 1-day all-day event ending at `00:00` on `tomorrowStart` must not appear in `tomorrow` (`evEnd > tomorrowStart`, strict `>`).
- **Today overflow suppresses tomorrow:** When `todayCount > maxRows - 1` (`> 7`), render `maxRows - 2` (`6`) events + `"+N weitere"` and return without drawing `"Morgen"`.
- **Zero remaining rows suppresses `"Morgen"`:** When today fits in `6` or `7` rows (`remainingRows <= 1`), `availTomorrow = remainingRows - 1 <= 0`, so `"Morgen"` is omitted rather than drawn orphaned.
- **Empty today keeps `"Heute"`:** When `todayCount == 0`, render `"Heute"` followed by `TXT_NO_EVENTS` (`"Keine Termine"`), then `"Morgen"` and tomorrow's events (or `TXT_NO_EVENTS`).

---

### Task 1: Commit Spec & Update Google Apps Script Bridge in `README.md`

**Files:**
- Commit: `docs/superpowers/specs/2026-11-20-calendar-today-tomorrow-design.md`
- Modify: `README.md:59-107`

**Interfaces:**
- Produces (HTTP GET JSON contract on `script.google.com`):
  - Request query params: `token`, `cal1`, `cal2`, `datetime` (optional ISO-8601 local timestamp; defaults to `new Date()`)
  - Response JSON: `{ "cal1": { "today": [{ "time": "HH:mm" | "", "title": string, "allDay": bool }], "tomorrow": [...] }, "cal2": { "today": [...], "tomorrow": [...] } }`

- [ ] **Step 1: Commit the approved design doc on `feature/calendar-today-tomorrow`**

```bash
git add docs/superpowers/specs/2026-11-20-calendar-today-tomorrow-design.md
git commit -m "docs(spec): add calendar today and tomorrow design"
```

- [ ] **Step 2: Update the Google Apps Script snippet in `README.md:65-100`**

Replace `formatEvents` / `doGet` with `formatEvent` + `splitEventsForTodayAndTomorrow(calId, from, todayEnd, tomorrowStart, tomorrowEnd)`:
- Parse `const parsed = e && e.parameter.datetime ? new Date(e.parameter.datetime) : new Date();`
- `const from = isNaN(parsed.getTime()) ? new Date() : parsed;`
- Compute `todayEnd` (`from`'s date at `23:59:59.999`), `tomorrowStart` (`from`'s `getDate() + 1` at `00:00:00.000`), and `tomorrowEnd` (`from`'s `getDate() + 1` at `23:59:59.999`).
- Call `cal.getEvents(from, tomorrowEnd)` once per calendar:
  - Push `formatEvent(ev)` to `today` if `ev.getStartTime() <= todayEnd && ev.getEndTime() > from`.
  - Push `formatEvent(ev)` to `tomorrow` if `ev.getStartTime() <= tomorrowEnd && ev.getEndTime() > tomorrowStart`.
- Return `{ cal1: ..., cal2: ... }` as JSON.

- [ ] **Step 3: Verify JS syntax and logic**

Review `README.md:59-107` to confirm all-day events on both days, multi-day all-day events, and ongoing events are handled without midnight bleed.

- [ ] **Step 4: Commit `README.md`**

```bash
git add README.md
git commit -m "docs(calendar): update Apps Script for today and tomorrow"
```

---

### Task 2: Update Localization Strings (`src/lang_gr.h` and `src/lang.h`)

**Files:**
- Modify: `src/lang_gr.h:74-78`
- Modify: `src/lang.h:74-78`

**Interfaces:**
- Produces (in both `src/lang_gr.h` and `src/lang.h`):
  - `const String TXT_CAL_EVENTS` (`"Termine"` / `"Events"`)
  - `const String TXT_TODAY` (`"Heute"` / `"Today"`)
  - `const String TXT_TOMORROW` (`"Morgen"` / `"Tomorrow"`)
  - `const String TXT_MORE_EVENTS` (`"weitere"` / `"more"`)

- [ ] **Step 1: Update calendar labels in `src/lang_gr.h:74-78`**

Replace `TXT_TODAY_EVENTS` with `TXT_CAL_EVENTS = "Termine";`, and add `TXT_TODAY = "Heute";`, `TXT_TOMORROW = "Morgen";`, and `TXT_MORE_EVENTS = "weitere";`.

- [ ] **Step 2: Update calendar labels in `src/lang.h:74-78`**

Replace `TXT_TODAY_EVENTS` with `TXT_CAL_EVENTS = "Events";`, and add `TXT_TODAY = "Today";`, `TXT_TOMORROW = "Tomorrow";`, and `TXT_MORE_EVENTS = "more";`.

- [ ] **Step 3: Verify identical symbol names across both language headers**

Run: `rg -n "TXT_CAL_EVENTS|TXT_TODAY|TXT_TOMORROW|TXT_MORE_EVENTS" src/lang.h src/lang_gr.h`
Expected: Both files define the exact same 4 symbol names.

---

### Task 3: Update Calendar Fetcher & Data Model (`src/calendar.h`)

**Files:**
- Modify: `src/calendar.h:10-107`

**Interfaces:**
- Consumes: `getLocalTime(&timeinfo)` (`time.h`), `calendarBridgeUrl`, `calendarBridgeToken`, `calendarId1`, `calendarId2`
- Produces:
  - `struct CalendarDayEvents { CalendarEvent events[MAX_CALENDAR_EVENTS]; int count = 0; };`
  - `struct CalendarData { CalendarDayEvents today; CalendarDayEvents tomorrow; };`
  - `extern CalendarData cal1Data;`
  - `extern CalendarData cal2Data;`
  - `String FormatLocalIsoDateTime()`
  - `bool DecodeCalendarJson(const String& payload, bool print)`
  - `bool ReceiveCalendars(bool print)`

- [ ] **Step 1: Replace global day arrays with `CalendarDayEvents` and `CalendarData` in `src/calendar.h:10-20`**

Define `CalendarDayEvents` and `CalendarData`, and declare `extern CalendarData cal1Data;` and `extern CalendarData cal2Data;`.

- [ ] **Step 2: Add `FormatLocalIsoDateTime()` in `src/calendar.h`**

Call `getLocalTime(&timeinfo, 1000)`; if unavailable, return `""`. Otherwise format `%Y-%m-%dT%H:%M:%S` and `%z`, inserting `:` into `±HHMM` so the result is RFC-3339 / ISO-8601 (`YYYY-MM-DDTHH:MM:SS+HH:MM`).

- [ ] **Step 3: Update `DecodeCalendarJson` and `ReceiveCalendars` in `src/calendar.h:52-107`**

- `DecodeCalendarJson`: populate `cal1Data.today` / `cal1Data.tomorrow` from `doc["cal1"]["today"]` / `doc["cal1"]["tomorrow"]`, and `cal2Data.today` / `cal2Data.tomorrow` from `doc["cal2"]["today"]` / `doc["cal2"]["tomorrow"]`. Log today and tomorrow counts for `cal1` and `cal2` when `print` is true.
- `ReceiveCalendars(bool print)`: rename from `ReceiveTodayCalendars`; append `&datetime=" + UrlEncode(dt)` when `FormatLocalIsoDateTime()` is non-empty.

---

### Task 4: Update Calendar Rendering, Wiring, and Version (`Station.ino`)

**Files:**
- Modify: `Station.ino:97` (`version`)
- Modify: `Station.ino:136-139` (globals)
- Modify: `Station.ino:174-180` (`setup()`)
- Modify: `Station.ino:280` (`DisplayWeather()`)
- Modify: `Station.ino:661-717` (`DrawCalendarListCard` & `DisplayCalendarSection`)
- Modify: `Station.ino:1535-1539` (changelog)

**Interfaces:**
- Consumes (`src/calendar.h`, `src/lang_gr.h` / `src/lang.h`):
  - `cal1Data`, `cal2Data`, `ReceiveCalendars(bool print)`
  - `TXT_CAL_EVENTS`, `TXT_TODAY`, `TXT_TOMORROW`, `TXT_NO_EVENTS`, `TXT_ALL_DAY`, `TXT_CAL_NOT_CONF`, `TXT_MORE_EVENTS`

- [ ] **Step 1: Update globals and `setup()` in `Station.ino:136-180`**

Replace `cal1Events` / `cal2Events` / `cal1EventCount` / `cal2EventCount` with `CalendarData cal1Data;` and `CalendarData cal2Data;`. Update `setup()` to call `ReceiveCalendars(true)`.

- [ ] **Step 2: Add subheader + event row helpers and update `DrawCalendarListCard` & `DisplayCalendarSection` in `Station.ino:661-717`**

Keep rendering clean by extracting a small helper `DrawCalendarSubheader(int x, int rowY, int w, int padX, const String& label)` (prints `label` in `u8g2_font_helvB10_tf` with a horizontal divider line from `x + padX + labelW + 6` to `x + w - padX` at `rowY - 4`, then restores `u8g2_font_helvB12_tf`) and `DrawCalendarEventRow(...)` so event row drawing is not duplicated between today and tomorrow.

Implement `DrawCalendarListCard(int x, int y, int w, int h, const char* title, const CalendarData& data, bool configured)`:
1. Draw outer rect, divider, and `title` (unchanged).
2. If `!configured`: print `TXT_CAL_NOT_CONF` and return.
3. Draw `TXT_TODAY` subheader at row `0` (`rowIdx = 0`, advance `rowIdx = 1`).
4. Today events:
   - `availToday = maxRows - rowIdx` (`7`).
   - If `data.today.count == 0`: print `TXT_NO_EVENTS` at `rowIdx++`.
   - Else if `data.today.count > availToday`: draw `availToday - 1` (`6`) events, print `"+" + String(data.today.count - (availToday - 1)) + " " + TXT_MORE_EVENTS` at the final row, and **return** (tomorrow omitted).
   - Else: draw all `data.today.count` events (`rowIdx += data.today.count`).
5. Tomorrow events:
   - `availTomorrow = maxRows - rowIdx - 1` (reserves 1 row for `TXT_TOMORROW`).
   - If `availTomorrow <= 0`: **return** (`"Morgen"` hidden).
   - Draw `TXT_TOMORROW` subheader at `rowIdx++`.
   - If `data.tomorrow.count == 0`: print `TXT_NO_EVENTS`.
   - Else if `data.tomorrow.count > availTomorrow`: draw `availTomorrow - 1` events and print `"+" + String(data.tomorrow.count - (availTomorrow - 1)) + " " + TXT_MORE_EVENTS`.
   - Else: draw all `data.tomorrow.count` events.

Update `DisplayCalendarSection` (`Station.ino:705-717`) to render `TXT_CAL_EVENTS` and pass `cal1Data` / `cal2Data`. Update `DisplayWeather()` comment at line 280.

- [ ] **Step 3: Bump `version` to `"1.14"` and add changelog entry at `Station.ino:97` and `Station.ino:1535-1539`**

- [ ] **Step 4: Build firmware with PlatformIO**

Run: `/Users/mb/.local/bin/pio run`
Expected: Exits `0` with `[SUCCESS]`.

- [ ] **Step 5: Commit implementation changes**

```bash
git add src/lang.h src/lang_gr.h src/calendar.h Station.ino
git commit -m "feat(calendar): display today and tomorrow events together"
```
