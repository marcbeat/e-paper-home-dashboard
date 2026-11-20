# NINA Public Warnings Grid Design

## Intent & Why
Display active civil protection and severe weather warnings from Germany's NINA (BBK) public API (`https://warnung.bund.de/api31/dashboard/{ARS}.json`) directly to the left of the 3-hour weather forecast. Up to four warnings render as individual high-visibility black-and-white cards in a 2×2 grid, prioritized by severity so critical alerts stand out immediately on the e-paper display.

## Goals, Non-Goals, and Anti-Goals

### Goals
- Add `ninaApiArs` configuration parameter to `src/config.example.h` (empty default) and `src/config.h` (`"059130000000"`).
- Implement `src/nina.h` to fetch `https://warnung.bund.de/api31/dashboard/{ARS}.json` over HTTPS, filter the JSON during deserialization, and sort alerts by severity descending.
- Render up to 4 individual hi-vis cards in a 2×2 grid (`x=5..360, y=450..630`) to the left of `DisplayForecastSection(372, 450)`, with a `"+N weitere"` (`"+N more"`) footer below the grid when more than 4 warnings exist.
- Render a calm single card when zero warnings are active (or `TXT_NOT_CONF` when `ninaApiArs` is empty, and an error indicator on fetch failure).
- Add localized strings in `src/lang_gr.h` and `src/lang.h`.
- Document the NINA API integration, `ninaApiArs`, and the regional key source (`https://www.dcat-ap.de/def/politicalGeocoding/regionalKey/`) in `README.md`.
- Bump `version` and append a changelog entry in `Station.ino`.

### Non-Goals
- No secondary requests to `/warnings/{id}.json` for full-text descriptions or instructions.
- No automatic ARS geocoding from `LAT`/`LON`.
- No automated unit test suite (repository has no test runner; verification is a PlatformIO build).

### Anti-Goals
- Do not deserialize large `area.data` polygon strings or unused `i18nTitle` languages into ESP32 heap.
- Do not overlap adjacent sections (`DisplayAstronomySection` ending at `y=438`, `DisplayForecastSection` starting at `x=372`, or `DisplayCalendarSection` starting at `y=674`).

## Constraints
- **Display:** 1-bit Black/White (`GxEPD2_BW`); high visibility relies on inverted black fills, thick frames, warning icons, and bold typography rather than color.
- **Geometry:** Left-middle slot `x=5..360` (`w=355`), `y=450..630` (`h=180`), with overflow footer space at `y=634..652`.
- **Platform:** ESP32 (`esp32dev`), `ArduinoJson` 7.4.3, `HTTPClient`, `WiFiClientSecure::setInsecure()` (10s timeout), matching `src/fuel.h` and `src/travel.h`.

## Approaches
1. **Severity-tiered B/W 2×2 grid + footer overflow (Chosen):** Draw only active warning cards in a 2×2 grid (`173×86` px each, 8px gaps) aligned with the 180px forecast row. Use solid black inverted cards for `Extreme`/`Severe` and thick-bordered cards with a black header strip for `Moderate`/`Minor`. When `>4` warnings exist, show the top 4 in the grid and `+N weitere` in the 34px margin below (`y=634..652`).
2. **Always-4-card grid + 4th-slot overflow:** Replace the 4th warning card with a `"+N weitere"` summary card when `>4` warnings exist. Rejected because it sacrifices a real warning slot unnecessarily when vertical margin exists below the grid.

## Design Details

### 1. Configuration & Fetch (`src/config.example.h`, `src/config.h`, `src/nina.h`)
- Config: `const char* ninaApiArs = "";` in `src/config.example.h` (`"059130000000"` in `src/config.h`).
- Endpoint: `https://warnung.bund.de/api31/dashboard/<ARS>.json`.
- `ArduinoJson` `DeserializationOption::Filter` retains only `id`, `sent`, `payload.data.headline`, `payload.data.provider`, `payload.data.severity`, and `i18nTitle.de` / `i18nTitle.en`.
- Severity rank: `Extreme=4`, `Severe=3`, `Moderate=2`, `Minor=1`, other/missing=`0`; ties broken by `sent` timestamp descending. Store top `MAX_NINA_WARNINGS = 4` plus `totalCount` and `valid` flag.
- `setup()` calls `ReceiveNinaWarnings(true)` (up to 2 attempts) when `HasConfiguredNina()` is true.

### 2. Hi-Vis Card Grid (`Station.ino`)
- `DisplayNinaSection(5, 450, 355, 180)`:
  - **Unconfigured / Fetch failed:** Single `355×180` card with `DrawInfoCardHeader(..., "NINA")` showing `TXT_NOT_CONF` or `TXT_NINA_FETCH_ERR`.
  - **0 warnings:** Single `355×180` calm card with `DrawInfoCardHeader(..., TXT_NINA_TITLE)` and centered `TXT_NINA_NO_WARNINGS` (`"Keine aktuellen Warnungen"` / `"No active warnings"`).
  - **1–4+ warnings:** 2×2 grid (`cardW = 173`, `cardH = 86`, `gap = 8`):
    - **`Extreme` / `Severe`:** Solid `GxEPD_BLACK` filled card with white warning triangle (`!`), white `provider` + severity tag in header, and up to 3 wrapped lines of white headline text.
    - **`Moderate` / `Minor` / `Unknown`:** 2px `GxEPD_BLACK` border, 22px solid black header bar with white `!` triangle + `provider` + severity tag, white body with up to 3 wrapped lines of black headline text.
    - **Overflow (`totalCount > 4`):** Render `"+<N> " + TXT_MORE_EVENTS` centered at `y=650` below the grid.

### 3. Documentation (`README.md`)
- Document NINA warning integration, `ninaApiArs` configuration, and that regional keys (ARS, 12-digit Kreis-level keys ending in `0000000`) can be obtained from `https://www.dcat-ap.de/def/politicalGeocoding/regionalKey/`.

## Builder Decisions Left Open
- Exact U8g2 extended-Latin font choice (`u8g2_font_helvB10_te` / `helvB08_te` / `helvB10_tf`) and line-wrapping helper sizing inside `173×86` px cards.
