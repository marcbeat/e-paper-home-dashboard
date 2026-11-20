# Fuel Prices & Travel Times Cards Design

## Intent & Context
Add two side-by-side informational cards—**Fuel Prices (`Kraftstoffpreise`)** and **Travel Times (`Fahrzeiten`)**—to the 1304×984 Waveshare e-Paper dashboard (`Station.ino`), placed in the currently empty horizontal band to the right of the Date/Time and Wi-Fi/Power Status column and above the 3-hour weather forecast boxes.

Users get at-a-glance local fuel prices (E5, E10, Diesel in €/L) for 3 stations and live car driving times (plus traffic delay) from home (`LAT`/`LON`) to 2 destinations alongside weather and today's calendar events.

## Goals, Non-Goals, and Anti-Goals

### Goals
- Fetch live E5, E10, and Diesel prices (€/L) for up to 3 configured stations via Tankerkönig `prices.php` in a single HTTPS request.
- Fetch real-time car driving travel times from `LAT`/`LON` to up to 2 configured destinations via Mapbox Directions API (`mapbox/driving-traffic`, `depart_at=now`, `overview=false`, `steps=false`), extracting both live `duration` and baseline `duration_typical`.
- Display two separate, side-by-side cards matching existing dashboard framing (`drawRect`, horizontal header divider, centered `helvB14` title) in the space right of Date/Time/Status (`y = 238..438`).
- Store all API keys, station UUIDs/labels, and destination coordinates/labels in `src/config.h` (with safe empty/placeholder defaults in `src/config.example.h`).
- Localize all UI strings in `src/lang_gr.h` and `src/lang.h`.

### Non-Goals
- No OAuth2 interactive login or refresh-token flow against PACE POI API (replaced by Tankerkönig).
- No extra Tankerkönig `detail.php` calls to discover station names dynamically (short labels are configured in `config.h`).
- No route polylines, turn-by-turn steps, or map rendering.
- No automated unit test suite (repository has no test runner; verification is PlatformIO build + serial/device inspection when connected).

### Anti-Goals
- Do not block weather or calendar rendering if fuel or travel APIs are unconfigured, time out, or return errors.
- Do not overlap the Date/Time/Status column when localized date strings widen `statusW`.
- Do not render broken Euro glyphs: `u8g2_font_helvB*_tf` lacks `€` (`U+20AC`), so cards needing `€` must use `u8g2_font_helvB12_te` / `u8g2_font_helvB14_te` (which include `U+20AC` and German umlauts).

## Constraints
- **Platform:** ESP32 (`esp32dev`, Arduino framework, `ArduinoJson` 7.4.3, `HTTPClient`, `WiFiClientSecure`).
- **Heap & TLS:** Mirror `src/calendar.h` by using `WiFiClientSecure::setInsecure()` with a 10s HTTP timeout and compact response flags (`overview=false&steps=false` on Mapbox) to preserve ESP32 heap.
- **Screen Coordinates (`1304×984`):**
  - Top weather row ends at `y = 230`.
  - Date/Time spans `x = 12 .. 5 + statusW`, `y ≈ 240..315`.
  - Status card spans `x = 5 .. 5 + statusW`, `y = 330..438`.
  - Astronomy + 3h Forecast row start at `y = 450` (Forecast starts at `x = 372`).
  - Available region: `cardsX = max(372, 5 + statusW + 14)`, `cardsY = 238`, `cardsRight = 1296`, `cardsH = 200` (`y = 238..438`), `gap = 12`.
- **Versioning & Git:** Follow `.agents/docs/VERSION-CONTROL.md` (`feature/...` branch, Conventional Commits) and bump `version` in `Station.ino` (`1.9` -> `1.10`) with a changelog entry.

## Approaches Considered (Fuel Prices Source)
Because `pace-poi-openapi-source.json` requires OAuth2 (`id.pace.cloud`, `authorizationCode` flow) on every `/gas-stations` endpoint and the user only has a standard PACE user account (no developer `client_id`), three options were evaluated:

1. **Tankerkönig API directly on ESP32 (Chosen):** Free static API key (`creativecommons.tankerkoenig.de/json/prices.php`), 1 HTTPS GET fetches E5/E10/Diesel for all 3 station UUIDs from Germany's official MTS-K feed—the same underlying price feed PACE uses, with zero OAuth2 complexity on ESP32.
2. **PACE POI API via OAuth2 `refresh_token` on ESP32:** ESP32 exchanges `client_id` + `refresh_token` on each wake-up, then makes 3 `GET /gas-stations/{id}` calls. Rejected because obtaining/maintaining developer client credentials and rotating refresh tokens adds fragility without price benefit.
3. **PACE POI API via external proxy/bridge:** Host a separate script/proxy handling PACE auth. Rejected to avoid extra external infrastructure when Tankerkönig works directly.

Travel Times has one direct path: **Mapbox Directions API (`mapbox/driving-traffic`)** using the configured Mapbox token and `LAT`/`LON` origin.

## Architecture & Data Design

### 1. Configuration (`src/config.example.h` & `src/config.h`)
Add non-empty/empty defaults cleanly (matching `calendarBridgeUrl` pattern):
```cpp
// Fuel prices (Tankerkönig API: https://creativecommons.tankerkoenig.de/)
const char* tankerkoenigApiKey = "";
const char* fuelStationIds[3]   = { "", "", "" }; // Station UUIDs
const char* fuelStationNames[3] = { "", "", "" }; // Short display names (e.g. "Aral", "Shell", "Jet")

// Travel times (Mapbox Directions API)
const char* mapboxAccessToken = "";
const char* travelDestNames[2] = { "", "" }; // Short display names (e.g. "Arbeit", "Zentrum")
const char* travelDestLats[2]  = { "", "" }; // Destination latitudes
const char* travelDestLons[2]  = { "", "" }; // Destination longitudes
```
*(Builder may represent these as parallel arrays or structs as long as `config.example.h` and `config.h` share identical declarations.)*

### 2. Fuel Fetcher (`src/fuel.h`)
- Data model (`FuelStationData`, array of 3):
  - `String name` (from `fuelStationNames[i]`)
  - `bool configured` (`strlen(fuelStationIds[i]) > 0`)
  - `bool valid` (API succeeded and station entry present)
  - `bool isOpen` (`status == "open"`)
  - `float e5`, `e10`, `diesel` (`-1.0f` when `false` or missing)
- `bool ReceiveFuelPrices(bool print)`:
  - Skip and return `false` quietly if `strlen(tankerkoenigApiKey) == 0` or all 3 `fuelStationIds` are empty.
  - Build comma-separated `ids` list from non-empty `fuelStationIds` and call `GET https://creativecommons.tankerkoenig.de/json/prices.php?ids=<ids>&apikey=<key>`.
  - Parse JSON with `ArduinoJson`, verify `doc["ok"] == true`, and populate each configured station from `doc["prices"][id]`.
  - Note: Tankerkönig returns `false` (boolean) instead of a number for unavailable fuel types or closed stations; check `is<float>()` / `is<double>()` before reading numeric price so `false` becomes `-1.0f` (`"--"`).
  - On HTTP/JSON failure, log to Serial and return `false`; `Station.ino` calls `AddToEventLog("*** Failed to Rx Fuel prices ***")` only when configured and all attempts fail (up to 2 attempts, matching `ReceiveTodayCalendars`).

### 3. Travel Time Fetcher (`src/travel.h`)
- Data model (`TravelRouteData`, array of 2):
  - `String name` (from `travelDestNames[i]`)
  - `bool configured` (name/lat/lon non-empty)
  - `bool valid`
  - `int durationMin` (rounded minutes from `routes[0].duration / 60.0`)
  - `int delayMin` (rounded `(duration - duration_typical) / 60.0`, clamped `>= 0`; `-1` if `duration_typical` absent)
- `bool ReceiveTravelTimes(bool print)`:
  - Skip if `strlen(mapboxAccessToken) == 0` or `LAT`/`LON` invalid or no destinations configured.
  - For each configured destination `i`:
    - Call `GET https://api.mapbox.com/directions/v5/mapbox/driving-traffic/<LON>,<LAT>;<destLon>,<destLat>?overview=false&steps=false&depart_at=now&access_token=<token>`.
    - *(Note: `depart_at=now` uses Mapbox's live real-time departure traffic; ESP32 has also synced NTP time in `SetupTime()`.)*
    - Parse `doc["code"] == "Ok"`, read `routes[0]["duration"]` and `routes[0]["duration_typical"]`.
  - Succeed if all configured destinations decode cleanly; on failure log to Serial and leave failed entry `valid = false` (`"--"`), with `AddToEventLog("*** Failed to Rx Travel times ***")` if configured and fetch fails (up to 2 attempts per destination or overall).

### 4. Display Layout (`Station.ino`)
In `DisplayWeather()`:
- Compute `cardsX = max(372, 5 + statusW + 14)`, `cardsY = 238`, `cardsH = 200`, `totalW = 1296 - cardsX`, `gap = 12`.
- Split `totalW - gap` roughly `55%` / `45%` (at `cardsX = 372`: `totalW = 924`, `fuelW ≈ 502`, `travelW ≈ 410`).
- **Card 1 (`DisplayFuelSection`):**
  - Frame `drawRect(x, y, w, h, GxEPD_BLACK)` + divider at `y + 36`.
  - Centered title (`u8g2_font_helvB14_te`): `TXT_FUEL_PRICES` (`"Kraftstoffpreise"` / `"Fuel Prices"`), or include `(€/L)` in column headers.
  - If unconfigured (`tankerkoenigApiKey` empty or no station IDs): show `TXT_NOT_CONF` (`"Nicht konfiguriert"`).
  - Otherwise: table header row (`Station`, `E5 €/L`, `E10 €/L`, `Diesel €/L` or `€/L` subtitle) in `helvB12_te`, followed by 3 rows (`rowStep ≈ 38`) in `helvB14_te`:
    - Truncated station label (`TruncateTextToWidth`) on left.
    - If `!valid`: `"--"` across price columns; if `!isOpen`: `TXT_FUEL_CLOSED` (`"geschlossen"`); else 3-decimal prices (`1.749`) right- or column-aligned.
- **Card 2 (`DisplayTravelSection`):**
  - Frame `drawRect(x, y, w, h, GxEPD_BLACK)` + divider at `y + 36`.
  - Centered title (`u8g2_font_helvB14_te`): `TXT_TRAVEL_TIMES` (`"Fahrzeiten"` / `"Travel Times"`).
  - If unconfigured: show `TXT_NOT_CONF`.
  - Otherwise: 2 destination blocks (`~75 px` vertical budget each):
    - Line 1 (`helvB14_te` / `helvB18_tf`): Destination label on left (truncated to fit) + live duration on right (`"24 min"` or `"1 h 12 m"` if >= 60 min).
    - Line 2 (`helvB12_te`): Traffic situation below or beside:
      - If `!valid`: `"--"`
      - Else if `delayMin <= 1`: `TXT_TRAFFIC_CLEAR` (`"Freie Fahrt"`)
      - Else if `delayMin < 10`: `"+X min "` + `TXT_TRAFFIC_LIGHT` (`"etwas Verkehr"` / `"Verzögerung"`)
      - Else: `"+X min "` + `TXT_TRAFFIC_HEAVY` (`"viel Verkehr"`)

### 5. Localization (`src/lang_gr.h` & `src/lang.h`)
Add matching constants to both files:
- `TXT_FUEL_PRICES` (`"Kraftstoffpreise"` / `"Fuel Prices"`)
- `TXT_FUEL_CLOSED` (`"geschlossen"` / `"closed"`)
- `TXT_TRAVEL_TIMES` (`"Fahrzeiten"` / `"Travel Times"`)
- `TXT_TRAFFIC_CLEAR` (`"Freie Fahrt"` / `"Clear"`)
- `TXT_TRAFFIC_DELAY` (`"Verzögerung"` / `"delay"`)
- `TXT_TRAFFIC_HEAVY` (`"Stau / viel Verkehr"` / `"Heavy traffic"`)
- `TXT_NOT_CONF` (`"Nicht konfiguriert"` / `"Not configured"`, reusing or unifying `TXT_CAL_NOT_CONF`)

## Builder Decisions Left Open
- Exact pixel column offsets, padding, and whether to format durations >= 60 min as `1h 12m` vs `72 min`.
- Exact ArduinoJson filter/buffer sizing if streaming `HTTPClient` or reading `getString()`.
- Exact `config.h` placeholder vs user-filled values (user supplies keys/UUIDs/coords in `src/config.h` after setup).
