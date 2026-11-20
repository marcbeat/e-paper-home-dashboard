# Google Calendar Integration Plan ("Heutige Termine" — Piet & Nati)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Populate the bottom-left `"Piet"` and `"Nati"` event lists under `"Heutige Termine"` with all of today's events from two separate Google Calendars on every wake cycle.

**Architecture:** Deploy a lightweight Google Apps Script Web App bridge in a Google account that has read access to both calendars (`calendarIdPiet`, `calendarIdNati`). The ESP32 performs a single HTTPS `GET` via `WiFiClientSecure` + `HTTPClient` (following Google's 302 redirect), parses the compact JSON payload with `ArduinoJson` (already in `platformio.ini`), populates `pietEvents[]` / `natiEvents[]`, and renders them in `DisplayCalendarSection()`.

**Tech Stack:** ESP32 Arduino (`WiFiClientSecure`, `HTTPClient`), `ArduinoJson@^7`, `time.h` (NTP/TZ already synced in `SetupTime()`), Google Apps Script (`CalendarApp` + `ContentService`).

---

## Approach Comparison

| Approach | ESP32 Requests / Wake | Recurring Events & DST | Flash / RAM Impact | Setup & Maintenance |
|---|---|---|---|---|
| **1. Google Apps Script Web App (Recommended)** | **1 HTTPS GET** | Handled by Google `CalendarApp` | Minimal (~`WiFiClientSecure`, reuses `ArduinoJson`) | ~20-line script deployed once; no token expiry |
| 2. Google Calendar REST API v3 (OAuth2 Refresh Token) | 3 HTTPS calls (1 token + 2 calendars) | Handled by API (`singleEvents=true`) | Medium (refresh flow + larger JSON) | Google Cloud project + OAuth consent (Testing mode tokens expire every 7 days unless published) |
| 3. Secret iCal (`.ics`) URLs | 2 HTTPS GETs | **Broken/fragile** (`RRULE`, `EXDATE`, `TZID`) | **High OOM risk** (full calendar history can be MBs) | Easiest URL copy, worst reliability on 320 KB RAM |

**Why Apps Script wins here:** `build.log` shows the ESP32 app partition is already at **71.9% Flash** and battery power matters (`SleepDuration = 30`, `StopWiFi()`). One small JSON response minimizes Wi-Fi/TLS awake time, avoids OAuth2 7-day token expiration, and guarantees accurate expansion of recurring events and `Europe/Berlin` DST.

*(Alternative note: If you prefer zero external scripts and already maintain a Google Cloud project in Production consent status, `src/calendar.h` can be swapped to OAuth2 v3 without changing `Station.ino`'s `CalendarEvent` struct or `DisplayCalendarSection()`.)*

---

## Global Constraints

- `src/config.h` holds all secrets and is git-ignored; `src/config.example.h` documents every config key.
- Existing config keys already added: `calendarBridgeUrl`, `calendarBridgeToken`, `calendarIdPiet`, `calendarIdNati`.
- Display capacity: `MAX_CALENDAR_EVENTS = 10` per list; UI shows up to 8 rows (`7 + "+N weitere"` on overflow).
- Network calls must run inside `setup()` **after** `StartWiFi() == WL_CONNECTED && SetupTime() == true` and **before** `StopWiFi()`.
- Calendar fetch failure must **not** prevent weather display (`RxWeather` still renders; log calendar errors via `AddToEventLog(...)`).

---

## Prerequisite: Google Apps Script Deployment (Manual, ~5 min)

1. Ensure Nati's Google Calendar is shared (at least "See all event details") with the Google account hosting the script.
2. Open [script.google.com](https://script.google.com) -> **New project**, paste:

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
    piet: formatEvents(e.parameter.piet || "", startOfDay, endOfDay),
    nati: formatEvents(e.parameter.nati || "", startOfDay, endOfDay),
  };
  return ContentService.createTextOutput(JSON.stringify(payload))
    .setMimeType(ContentService.MimeType.JSON);
}
```

3. Click **Deploy -> New deployment -> Web app**:
   - Execute as: **Me**
   - Who has access: **Anyone** (gated by `SHARED_TOKEN`)
4. Copy the `/exec` URL into `calendarBridgeUrl`, `SHARED_TOKEN` into `calendarBridgeToken`, and both Calendar IDs into `calendarIdPiet` / `calendarIdNati` in `src/config.h`.

---

### Task 1: Add URL-encoding & JSON calendar fetch/decode module (`src/calendar.h`)

**Files:**
- Create: `src/calendar.h`

**Interfaces:**
- Consumes from `Station.ino` / `config.h`:
  - `calendarBridgeUrl`, `calendarBridgeToken`, `calendarIdPiet`, `calendarIdNati` (`const char*`)
  - `struct CalendarEvent { String time; String title; bool allDay; };`
  - `MAX_CALENDAR_EVENTS`, `pietEvents[]`, `natiEvents[]`, `pietEventCount`, `natiEventCount`
- Produces:
  - `bool ReceiveTodayCalendars(bool print);`
  - `bool DecodeCalendarJson(const String& payload, bool print);`

- [ ] **Step 1:** Create `src/calendar.h` with `UrlEncode(const String& value)` helper for query parameters (`token`, `piet`, `nati`).
- [ ] **Step 2:** Implement `DecodeCalendarJson(const String& payload, bool print)`:
  - Parse JSON with `ArduinoJson` `JsonDocument doc`.
  - Check `doc["error"]` (log and return `false` if present).
  - Write a shared helper `PopulateEventList(JsonArrayConst arr, CalendarEvent out[], int& outCount)` to fill `pietEvents`/`natiEvents` up to `MAX_CALENDAR_EVENTS` without duplicating loop logic.
- [ ] **Step 3:** Implement `ReceiveTodayCalendars(bool print)`:
  - Return `false` immediately if `strlen(calendarBridgeUrl) == 0`.
  - Use `WiFiClientSecure` (`client.setInsecure()` or Google Trust Services root CA; document tradeoff in comment) + `HTTPClient http`.
  - Enable `http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)` and `http.setTimeout(10000)` (required for Google Apps Script 302 redirect to `script.googleusercontent.com`).
  - Call `GET`, check `httpCode == HTTP_CODE_OK`, pass `http.getString()` to `DecodeCalendarJson(...)`, call `http.end()`.

---

### Task 2: Wire `ReceiveTodayCalendars` into `Station.ino` boot cycle

**Files:**
- Modify: `Station.ino` (`#include "calendar.h"`, `setup()`, changelog)

- [ ] **Step 1:** Move `struct CalendarEvent` + `MAX_CALENDAR_EVENTS` above `#include "calendar.h"` (or define them in `src/calendar.h` and include `calendar.h` right after `#include "common.h"`).
- [ ] **Step 2:** In `setup()`, right after `ReceiveOneCallWeather` succeeds (or independently before `StopWiFi()`), call `ReceiveTodayCalendars(true)` when `strlen(calendarBridgeUrl) > 0`; on failure call `AddToEventLog("*** Failed to Rx Calendar data ***")` so weather still renders.
- [ ] **Step 3:** Bump `version` to `"1.8"` and add `Version 1.8` changelog entry at bottom of `Station.ino`.

---

### Task 3: Compile, Flash-budget check, and Device Verification

- [ ] **Step 1:** Run `pio run` and verify compilation succeeds and Flash usage stays comfortably below `90%` (`~1.18 MB` of `1.31 MB` app partition, since `WiFiClientSecure` adds TLS code).
  - *Fallback if Flash > 95%:* switch `board_build.partitions` in `platformio.ini` to `huge_app.csv` (3 MB app, no OTA).
- [ ] **Step 2:** Flash to ESP32 (`pio run -t upload`) and inspect `pio device monitor` for:
  - HTTP `200` from Apps Script bridge
  - Decoded event counts for Piet and Nati
  - `Available heap` and `Min free heap` after TLS + display render
- [ ] **Step 3:** Visually inspect the 12.48" e-ink screen for `"Heutige Termine"`, `"Piet"`, `"Nati"`, umlauts (`ä/ö/ü/ß`), all-day vs `HH:MM` alignment, and right-aligned 2-chart forecast alignment.
- [ ] **Step 4:** Commit changes on the `feature/...` branch following `.agents/docs/VERSION-CONTROL.md`.
