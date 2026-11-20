# Fuel Prices & Travel Times Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two side-by-side dashboard cards—Fuel Prices (`Kraftstoffpreise`, Tankerkönig `prices.php`) and Travel Times (`Fahrzeiten`, Mapbox `driving-traffic`)—to the right of Date/Time/Status on the 1304×984 e-Paper display.

**Architecture:** Modularize each integration into a dedicated header (`src/fuel.h`, `src/travel.h`) mirroring `src/calendar.h`, add localized labels in `src/lang_gr.h` / `src/lang.h` and credentials/locations in `src/config.example.h` / `src/config.h`, then wire data fetching into `setup()` and card rendering into `DisplayWeather()` in `Station.ino`.

**Tech Stack:** ESP32 (`esp32dev`, Arduino), `WiFiClientSecure`, `HTTPClient`, `ArduinoJson` 7.4.3, `GxEPD2` 1.6.9, `U8g2_for_Adafruit_GFX` 1.8.0 (`_te` extended Helvetica Bold fonts).

**Spec:** [docs/superpowers/specs/2026-11-20-fuel-and-travel-design.md](file:///c:/Users/marc/Documents/PlatformIO/Projects/eink-station/docs/superpowers/specs/2026-11-20-fuel-and-travel-design.md)

## Global Constraints

- PlatformIO target: `esp32dev` (`espressif32`, Arduino framework); build command: `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run` (must exit 0 with `SUCCESS`).
- HTTPS transport: `WiFiClientSecure::setInsecure()` and `http.setTimeout(10000)` matching `src/calendar.h`.
- Config hygiene: `src/config.h` is git-ignored; keep identical variable declarations in `src/config.example.h` and `src/config.h`, and never stage/commit `src/config.h`.
- Euro & umlaut glyph safety: use `u8g2_font_helvB12_te` / `u8g2_font_helvB14_te` (not `_tf`) in cards displaying `€` (`U+20AC`) or German text.
- Display region: `cardsX = max(372, 5 + statusW + 14)`, `cardsY = 238`, `cardsRight = 1296`, `cardsH = 200`, `gap = 12`.
- Git & versioning: stay on branch `feature/fuel-and-travel`, use Conventional Commits (50/72 rule), and bump `version` in `Station.ino` from `"1.9"` to `"1.10"`.

## Review Focus

- **Tankerkönig boolean `false` prices:** Closed stations or missing fuel types return `"e5": false` instead of a number; `DecodeFuelJson` must check `is<float>()` / `is<double>()` so `false` maps to `-1.0f` (`"--"`), never `0.000`.
- **Wide localized date string:** `cardsX = max(372, 5 + statusW + 14)` and proportional widths (`~55%` / `~45%` of `1296 - cardsX - 12`) prevent cards from colliding with Date/Time/Status.
- **Euro glyph rendering:** `DisplayFuelSection` must use `_te` font variants (`helvB12_te`, `helvB14_te`) so `€/L` renders instead of a missing glyph.
- **Partial configuration & partial API errors:** Empty station/destination slots are skipped cleanly; one failing Mapbox request or closed station does not clear valid sibling entries or block weather/calendar rendering.
- **Missing or negative `duration_typical`:** If Mapbox omits `duration_typical`, `delayMin` is `-1` (render duration without bogus delay); if `duration <= duration_typical`, clamp `delayMin` to `0` (`TXT_TRAFFIC_CLEAR`).

---

### Task 1: Configuration Schema & Localization Strings

**Files:**
- Modify: `src/config.example.h:47-52`
- Modify (local only, git-ignored): `src/config.h`
- Modify: `src/lang_gr.h:73-77`
- Modify: `src/lang.h:73-78`

**Interfaces:**
- Produces (in `src/config.example.h` & `src/config.h`):
  - `const char* tankerkoenigApiKey`
  - `const char* fuelStationIds[3]`
  - `const char* fuelStationNames[3]`
  - `const char* mapboxAccessToken`
  - `const char* travelDestNames[2]`
  - `const char* travelDestLats[2]`
  - `const char* travelDestLons[2]`
- Produces (in `src/lang_gr.h` & `src/lang.h`):
  - `const String TXT_FUEL_PRICES`
  - `const String TXT_FUEL_CLOSED`
  - `const String TXT_TRAVEL_TIMES`
  - `const String TXT_TRAFFIC_CLEAR`
  - `const String TXT_TRAFFIC_DELAY`
  - `const String TXT_TRAFFIC_HEAVY`
  - `const String TXT_NOT_CONF`

- [ ] **Step 1: Add Tankerkönig and Mapbox configuration fields to `src/config.example.h`**

Append after line 52 (`calendarId2`) with setup comments and empty string defaults:
```cpp
// Fuel prices (Tankerkoenig API: https://creativecommons.tankerkoenig.de/)
const char* tankerkoenigApiKey  = "";
const char* fuelStationIds[3]   = { "", "", "" }; // Up to 3 station UUIDs
const char* fuelStationNames[3] = { "", "", "" }; // Short display labels (e.g. "Aral", "Shell", "Jet")

// Travel times (Mapbox Directions API: https://docs.mapbox.com/api/navigation/directions/)
const char* mapboxAccessToken   = "";
const char* travelDestNames[2]  = { "", "" }; // Short destination labels (e.g. "Arbeit", "Zentrum")
const char* travelDestLats[2]   = { "", "" }; // Destination latitudes
const char* travelDestLons[2]   = { "", "" }; // Destination longitudes
```

- [ ] **Step 2: Mirror the exact same declarations with empty defaults in `src/config.h`**

Append the declarations from Step 1 to the end of `src/config.h` without touching existing credentials.

- [ ] **Step 3: Add German and English UI strings to `src/lang_gr.h` and `src/lang.h`**

In `src/lang_gr.h`:
- `TXT_FUEL_PRICES  = "Kraftstoffpreise"`
- `TXT_FUEL_CLOSED  = "geschlossen"`
- `TXT_TRAVEL_TIMES = "Fahrzeiten"`
- `TXT_TRAFFIC_CLEAR = "Freie Fahrt"`
- `TXT_TRAFFIC_DELAY = "Verzögerung"`
- `TXT_TRAFFIC_HEAVY = "Viel Verkehr"`
- `TXT_NOT_CONF     = "Nicht konfiguriert"`

In `src/lang.h`:
- `TXT_FUEL_PRICES  = "Fuel Prices"`
- `TXT_FUEL_CLOSED  = "closed"`
- `TXT_TRAVEL_TIMES = "Travel Times"`
- `TXT_TRAFFIC_CLEAR = "Clear"`
- `TXT_TRAFFIC_DELAY = "delay"`
- `TXT_TRAFFIC_HEAVY = "Heavy traffic"`
- `TXT_NOT_CONF     = "Not configured"`

- [ ] **Step 4: Build to verify declarations compile cleanly**

Run: `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `========================= [SUCCESS]` on `esp32dev`.

- [ ] **Step 5: Commit tracked config template and localization changes**

Verify `git status` does not list `src/config.h`, then commit:
```powershell
git add src/config.example.h src/lang_gr.h src/lang.h
git commit -m "feat(config): add fuel and travel config and labels"
```

---

### Task 2: Tankerkönig Fuel Price Fetcher (`src/fuel.h`)

**Files:**
- Create: `src/fuel.h`
- Modify: `Station.ino:119-121,134-138,169-177`

**Interfaces:**
- Consumes: `tankerkoenigApiKey`, `fuelStationIds`, `fuelStationNames` (from `config.h`), `UrlEncode` (from `calendar.h`)
- Produces (in `src/fuel.h`):
  - `#define MAX_FUEL_STATIONS 3`
  - `struct FuelStationData { String name; bool configured; bool valid; bool isOpen; float e5; float e10; float diesel; };`
  - `extern FuelStationData fuelStations[MAX_FUEL_STATIONS];`
  - `bool HasConfiguredFuelStations()`
  - `bool DecodeFuelJson(const String& payload, bool print)`
  - `bool ReceiveFuelPrices(bool print)`

- [ ] **Step 1: Create `src/fuel.h` with data model and `HasConfiguredFuelStations()`**

Initialize each `fuelStations[i]` cleanly inside `ResetFuelStations()` (called at start of `ReceiveFuelPrices` and `DecodeFuelJson`): `name` falls back to `fuelStationNames[i]` (or `"Station " + String(i + 1)` if empty), `configured = (strlen(fuelStationIds[i]) > 0)`, `valid = false`, `isOpen = false`, `e5 = e10 = diesel = -1.0f`. `HasConfiguredFuelStations()` returns `true` when `strlen(tankerkoenigApiKey) > 0` and at least one `fuelStationIds[i]` is non-empty.

- [ ] **Step 2: Implement `DecodeFuelJson(const String& payload, bool print)` in `src/fuel.h`**

Parse `payload` into `JsonDocument doc`. Return `false` on `DeserializationError` or when `!(doc["ok"] | false)`. Read `JsonObjectConst prices = doc["prices"].as<JsonObjectConst>()`. For each configured `i`, look up `JsonObjectConst entry = prices[fuelStationIds[i]].as<JsonObjectConst>()`; if `!entry.isNull()`, set `valid = true`, `isOpen = (String(entry["status"] | "") == "open")`, and extract `e5`/`e10`/`diesel` via a helper `ExtractFuelPrice(JsonVariantConst v)` that returns `v.as<float>()` only when `v.is<float>() || v.is<double>()` (returning `-1.0f` when boolean `false` or null).

- [ ] **Step 3: Implement `ReceiveFuelPrices(bool print)` in `src/fuel.h`**

If `!HasConfiguredFuelStations()`, call `ResetFuelStations()` and return `false`. Build comma-separated `ids` of non-empty `fuelStationIds[i]`, request `https://creativecommons.tankerkoenig.de/json/prices.php?ids=<encoded>&apikey=<encoded>` using `WiFiClientSecure` (`setInsecure()`) and `HTTPClient` (`setTimeout(10000)`), check `HTTP_CODE_OK`, and return `DecodeFuelJson(payload, print)`.

- [ ] **Step 4: Wire `src/fuel.h` into `Station.ino`**

Include `"fuel.h"` after `#include "calendar.h"`, declare `FuelStationData fuelStations[MAX_FUEL_STATIONS];`, and in `setup()` (inside `if (RxWeather)` before `StopWiFi()`) if `HasConfiguredFuelStations()` try `ReceiveFuelPrices(true)` up to 2 attempts, calling `AddToEventLog("*** Failed to Rx Fuel prices ***")` if still false.

- [ ] **Step 5: Build and commit**

Run: `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `========================= [SUCCESS]` on `esp32dev`.
```powershell
git add src/fuel.h Station.ino
git commit -m "feat(fuel): add Tankerkoenig fuel price fetcher"
```

---

### Task 3: Mapbox Travel Time Fetcher (`src/travel.h`)

**Files:**
- Create: `src/travel.h`
- Modify: `Station.ino:119-122,134-139,169-178`

**Interfaces:**
- Consumes: `mapboxAccessToken`, `travelDestNames`, `travelDestLats`, `travelDestLons`, `LAT`, `LON` (from `config.h`), `UrlEncode` (from `calendar.h`)
- Produces (in `src/travel.h`):
  - `#define MAX_TRAVEL_DESTS 2`
  - `struct TravelRouteData { String name; bool configured; bool valid; int durationMin; int delayMin; };`
  - `extern TravelRouteData travelRoutes[MAX_TRAVEL_DESTS];`
  - `bool HasConfiguredTravelDests()`
  - `bool DecodeTravelJson(const String& payload, int index, bool print)`
  - `bool ReceiveTravelTimes(bool print)`

- [ ] **Step 1: Create `src/travel.h` with data model and `HasConfiguredTravelDests()`**

Implement `ResetTravelRoutes()` setting `name = travelDestNames[i]` (fallback `"Ziel " + String(i + 1)`), `configured = (strlen(travelDestLats[i]) > 0 && strlen(travelDestLons[i]) > 0)`, `valid = false`, `durationMin = -1`, `delayMin = -1`. `HasConfiguredTravelDests()` requires `strlen(mapboxAccessToken) > 0`, non-empty `LAT`/`LON`, and at least one configured destination.

- [ ] **Step 2: Implement `DecodeTravelJson(const String& payload, int index, bool print)` in `src/travel.h`**

Parse `payload` into `JsonDocument doc`. Return `false` on error, if `String(doc["code"] | "") != "Ok"`, or if `doc["routes"][0]` is null. Read `double durSec = route["duration"] | -1.0`; if `durSec < 0`, return `false`. Set `travelRoutes[index].durationMin = (int)round(durSec / 60.0)`. Check `JsonVariantConst typVar = route["duration_typical"]`: if `typVar.is<float>() || typVar.is<double>() || typVar.is<long>()`, compute `int delta = (int)round((durSec - typVar.as<double>()) / 60.0); travelRoutes[index].delayMin = (delta > 0) ? delta : 0;`, else `travelRoutes[index].delayMin = -1`. Mark `valid = true` and return `true`.

- [ ] **Step 3: Implement `ReceiveTravelTimes(bool print)` in `src/travel.h`**

Call `ResetTravelRoutes()`. If `!HasConfiguredTravelDests()`, return `false`. Reuse a `WiFiClientSecure` (`setInsecure()`) and `HTTPClient` (`setTimeout(10000)`). For each configured destination `i`, request:
`https://api.mapbox.com/directions/v5/mapbox/driving-traffic/<LON>,<LAT>;<destLon>,<destLat>?overview=false&steps=false&depart_at=now&access_token=<token>`
Retry a single failed destination up to 2 attempts; return `true` if all configured destinations succeeded (leaving any failed slot `valid = false` without aborting siblings).

- [ ] **Step 4: Wire `src/travel.h` into `Station.ino`**

Include `"travel.h"` after `#include "fuel.h"`, declare `TravelRouteData travelRoutes[MAX_TRAVEL_DESTS];`, and in `setup()` (inside `if (RxWeather)` before `StopWiFi()`) if `HasConfiguredTravelDests()` call `ReceiveTravelTimes(true)` and log `AddToEventLog("*** Failed to Rx Travel times ***")` on `false`.

- [ ] **Step 5: Build and commit**

Run: `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `========================= [SUCCESS]` on `esp32dev`.
```powershell
git add src/travel.h Station.ino
git commit -m "feat(travel): add Mapbox travel time fetcher"
```

---

### Task 4: Render Side-by-Side Cards & Bump Version (`Station.ino`)

**Files:**
- Modify: `Station.ino:97,239-258,660-664,1322-1325`

**Interfaces:**
- Consumes: `fuelStations`, `HasConfiguredFuelStations` (`src/fuel.h`); `travelRoutes`, `HasConfiguredTravelDests` (`src/travel.h`); `TXT_*` strings (`src/lang*.h`); `DrawCenteredText`, `TruncateTextToWidth` (`Station.ino`)
- Produces:
  - `void DisplayFuelSection(int x, int y, int width, int height)`
  - `void DisplayTravelSection(int x, int y, int width, int height)`
  - `String FormatTravelDuration(int minutes)`

- [ ] **Step 1: Implement `DisplayFuelSection(int x, int y, int width, int height)` in `Station.ino`**

Draw outer frame `display.drawRect(x, y, width, height, GxEPD_BLACK)` and divider `y + 36`. Set `u8g2Fonts.setFont(u8g2_font_helvB14_te)` and call `DrawCenteredText(x + width / 2, y + 25, TXT_FUEL_PRICES)`.
If `!HasConfiguredFuelStations()`, print `TXT_NOT_CONF` with `u8g2_font_helvB12_te` at `(x + 12, y + 66)` and return.
Otherwise render column headers (`E5 €/L`, `E10 €/L`, `Diesel €/L`) in `u8g2_font_helvB12_te` at `y + 58` right-aligned via `u8g2Fonts.getUTF8Width(...)` + `u8g2Fonts.setCursor(...)` (do not use `drawString(..., RIGHT)`, which measures Adafruit GFX bounds instead of U8g2 font metrics), and 3 station rows (`rowY = y + 92 + i * 36`) in `u8g2_font_helvB14_te`:
- Left: truncated station name (`TruncateTextToWidth(fuelStations[i].name, nameMaxW)`), skip row if `!fuelStations[i].configured` (or show `"--"`).
- If configured and `!fuelStations[i].valid`: print `"--"` in the price area.
- Else if `!fuelStations[i].isOpen`: print `TXT_FUEL_CLOSED` across the price columns.
- Else: print `e5`, `e10`, `diesel` formatted with `String(price, 3)` when `price > 0` or `"--"` when `< 0`.

- [ ] **Step 2: Implement `FormatTravelDuration(int minutes)` and `DisplayTravelSection(int x, int y, int width, int height)` in `Station.ino`**

`FormatTravelDuration(int minutes)`: returns `"--"` if `minutes < 0`, `String(minutes) + " min"` if `< 60`, or `String(minutes / 60) + " h " + String(minutes % 60) + " m"` if `>= 60`.
In `DisplayTravelSection`: draw frame and divider at `y + 36`, centered `TXT_TRAVEL_TIMES` with `u8g2_font_helvB14_te`. If `!HasConfiguredTravelDests()`, print `TXT_NOT_CONF` and return.
For each `i = 0..1`: if `!travelRoutes[i].configured`, skip; let `blockY = y + 66 + i * 74`:
- Row 1 (`u8g2_font_helvB14_te`): left-aligned truncated `travelRoutes[i].name`; right-aligned `FormatTravelDuration(travelRoutes[i].durationMin)` (or `"--"` if `!valid`) positioned with `u8g2Fonts.getUTF8Width(...)`.
- Row 2 (`u8g2_font_helvB12_te`, `blockY + 24`): if `!valid` print `"--"`; else if `delayMin < 0` print `""` (or `"Live"`); else if `delayMin <= 1` print `TXT_TRAFFIC_CLEAR`; else if `delayMin < 10` print `"+" + String(delayMin) + " min " + TXT_TRAFFIC_DELAY`; else print `"+" + String(delayMin) + " min " + TXT_TRAFFIC_HEAVY`.

- [ ] **Step 3: Call both card renderers in `DisplayWeather()`**

Inside `DisplayWeather()` in `Station.ino`, compute:
```cpp
int cardsX = max(372, 5 + statusW + 14);
int cardsY = 238;
int cardsH = 200;
int cardsGap = 12;
int cardsTotalW = 1296 - cardsX;
int fuelW = (cardsTotalW - cardsGap) * 55 / 100;
int travelW = cardsTotalW - cardsGap - fuelW;
```
Call `DisplayFuelSection(cardsX, cardsY, fuelW, cardsH);` and `DisplayTravelSection(cardsX + fuelW + cardsGap, cardsY, travelW, cardsH);`.

- [ ] **Step 4: Bump `version` to `"1.10"` and append Version 1.10 changelog at bottom of `Station.ino`**

Update `String version = "1.10";` at line 97 and document the two new cards in the changelog block at lines 1322–1325.

- [ ] **Step 5: Build and commit**

Run: `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `========================= [SUCCESS]` on `esp32dev`.
```powershell
git add Station.ino
git commit -m "feat(ui): render fuel price and travel time cards"
```
