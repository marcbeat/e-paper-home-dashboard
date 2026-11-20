# NINA Public Warnings Grid Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fetch German civil protection and weather warnings from the public NINA API (`/api31/dashboard/{ARS}.json`) and render up to 4 severity-prioritized hi-vis B/W cards in a 2×2 grid to the left of the hourly weather forecast, plus document the integration and regional key lookup in `README.md`.

**Architecture:** Add `ninaApiArs` to `config.example.h` / `config.h`, implement `src/nina.h` following the existing `src/fuel.h` / `src/travel.h` pattern (`WiFiClientSecure::setInsecure()`, `HTTPClient`, `ArduinoJson` v7 with `DeserializationOption::Filter`), and wire `ReceiveNinaWarnings(true)` into `setup()`. Render a 2×2 grid (`DisplayNinaSection(5, 450, 355, 180)`) in `Station.ino` using inverted black cards for `Extreme`/`Severe`, thick-framed black-header cards for `Moderate`/`Minor`, a `"+N weitere"` footer below when `totalCount > 4`, and a calm single card when zero warnings are active.

**Tech Stack:** ESP32 (`esp32dev`), Arduino framework, PlatformIO (`/Users/mb/.local/bin/pio run`), `ArduinoJson` 7.4.3, `HTTPClient`, `WiFiClientSecure`, `GxEPD2_BW` (1304×984 1-bit B/W), `U8g2_for_Adafruit_GFX`.

**Spec:** [2026-11-20-nina-warnings-design.md](file:///Users/mb/repos/eink-station/docs/superpowers/specs/2026-11-20-nina-warnings-design.md)

## Global Constraints

- Display resolution is `1304×984`, strictly 1-bit Black/White (`GxEPD_BLACK` / `GxEPD_WHITE`).
- Left-middle NINA grid occupies `x=5..360` (`w=355`), `y=450..630` (`h=180`), with overflow footer at `y=634..652` — must not overlap Astronomy (`y<=438`), Forecast (`x>=372`), or Calendar (`y>=664`).
- `src/config.h` is git-ignored; never stage or commit `src/config.h` or modify `platformio.ini`.
- Verification command: `/Users/mb/.local/bin/pio run` (compile check; no automated test suite in repo; `upload_port` requires hardware).
- Follow `.agents/docs/VERSION-CONTROL.md` Conventional Commits (imperative mood, <=50 char subject) on branch `feature/nina-warnings`.

## Review Focus

- **Empty `[]` response vs. fetch/parse failure:** An empty JSON array `[]` means zero active warnings (`ninaData.valid = true, totalCount = 0`), whereas HTTP non-200 or JSON parse errors must set `ninaData.valid = false` and show an error state rather than "no warnings".
- **Large payloads (`area.data` polygons & >4 warnings):** `ArduinoJson` `DeserializationOption::Filter` must discard `area.data`, `hash`, and unused locales during parsing so heap usage stays low even with many warnings.
- **Missing/partial alert fields:** Alerts lacking `i18nTitle.de`, `payload.data.headline`, `provider`, or `severity` must fall back safely without null-pointer crashes or blank cards.
- **Umlaut rendering & color-state leak:** Use `_te` fonts (`u8g2_font_helvB10_te`, `u8g2_font_helvB08_te`) and UTF-8-aware `TruncateTextToWidth` for German text, and always restore `u8g2Fonts.setForegroundColor(GxEPD_BLACK)` / `setBackgroundColor(GxEPD_WHITE)` after drawing inverted white-on-black elements.
- **Unconfigured or whitespace ARS:** Trim `ninaApiArs` in `HasConfiguredNina()`; when empty, skip HTTP requests in `setup()` and display `TXT_NOT_CONF`.

---

### Task 1: Add `ninaApiArs` Config and NINA Fetch/Decode Module (`src/nina.h`)

**Files:**
- Modify: `src/config.example.h:63-68`
- Modify: `src/config.h` (git-ignored; add `ninaApiArs = "059130000000"`)
- Create: `src/nina.h`
- Modify: `Station.ino:120-195` (`#include "nina.h"`, `NinaData ninaData`, `setup()` fetch)

**Interfaces:**
- Consumes: `ninaApiArs` (`const char*`), `Language` (`String`), `UrlEncode(const String&)` from `src/calendar.h`, `AddToEventLog(const String&)` in `Station.ino`
- Produces:
  - `#define MAX_NINA_WARNINGS 4`
  - `struct NinaWarning { String headline; String provider; String severity; int severityRank; String sent; };`
  - `struct NinaData { NinaWarning warnings[MAX_NINA_WARNINGS]; int totalCount; bool valid; };`
  - `extern NinaData ninaData;`
  - `inline bool HasConfiguredNina()`
  - `inline int NinaSeverityRank(const String& severity)`
  - `inline bool DecodeNinaJson(const String& payload, bool print)`
  - `inline bool ReceiveNinaWarnings(bool print)`

- [ ] **Step 1: Add `ninaApiArs` to `src/config.example.h` and `src/config.h`**

Append to `src/config.example.h`:
```cpp
// NINA civil protection & weather warnings (https://warnung.bund.de/api31/dashboard/{ARS}.json)
// Regional keys (ARS): https://www.dcat-ap.de/def/politicalGeocoding/regionalKey/
const char* ninaApiArs = ""; // 12-digit Amtlicher Regionalschluessel (Kreis level, e.g. "059130000000")
```
And add `const char* ninaApiArs = "059130000000";` at the bottom of `src/config.h` (do not commit `src/config.h`).

- [ ] **Step 2: Create `src/nina.h` with filtered JSON decoding, severity sorting, and HTTPS fetch**

Implement in `src/nina.h`:
1. `ResetNinaData()`: sets `ninaData.valid = false`, `ninaData.totalCount = 0`, clears `warnings[0..3]`.
2. `HasConfiguredNina()`: returns `true` when `ninaApiArs != nullptr` and trimmed `String(ninaApiArs)` is non-empty.
3. `NinaSeverityRank(const String& severity)`: case-insensitive mapping: `"Extreme" -> 4`, `"Severe" -> 3`, `"Moderate" -> 2`, `"Minor" -> 1`, else `0`.
4. `ExtractNinaHeadline(JsonObjectConst item)` helper: if `Language == "EN"`, prefer `item["i18nTitle"]["en"]`, then `item["payload"]["data"]["headline"]`, then `item["i18nTitle"]["de"]`; otherwise prefer `item["i18nTitle"]["de"]`, then `item["payload"]["data"]["headline"]`, then `item["i18nTitle"]["en"]`. Trim and fall back to `"Warnung"` if empty.
5. `DecodeNinaJson(const String& payload, bool print)`:
   - Build a static/local `JsonDocument filter` selecting `filter[0]["sent"]`, `filter[0]["payload"]["data"]["headline"]`, `filter[0]["payload"]["data"]["provider"]`, `filter[0]["payload"]["data"]["severity"]`, `filter[0]["i18nTitle"]["de"]`, `filter[0]["i18nTitle"]["en"]`.
   - Call `deserializeJson(doc, payload, DeserializationOption::Filter(filter))`. On error or if `!doc.is<JsonArrayConst>()`, log and return `false`.
   - Iterate array items, compute `NinaWarning` candidate (`severityRank = NinaSeverityRank(severity)`), increment `totalCount`, and maintain the top `MAX_NINA_WARNINGS` (4) sorted descending by `(severityRank, sent)` via insertion into `ninaData.warnings[]` (no extra dynamic array needed for >4 items!).
   - Set `ninaData.valid = true`, log count if `print`, and return `true`.
6. `ReceiveNinaWarnings(bool print)`:
   - `ResetNinaData()`; return `false` if `!HasConfiguredNina()`.
   - Trim `ninaApiArs`, build URL `"https://warnung.bund.de/api31/dashboard/" + UrlEncode(ars) + ".json"`.
   - Use `WiFiClientSecure` (`setInsecure()`) + `HTTPClient` (`setTimeout(10000)`), check `HTTP_CODE_OK`, call `DecodeNinaJson(http.getString(), print)`.

- [ ] **Step 3: Wire `src/nina.h` into `Station.ino`**

In `Station.ino`:
- Add `#include "nina.h"` right after `#include "travel.h"`.
- Declare `NinaData ninaData;` right after `TravelRouteData travelRoutes[MAX_TRAVEL_DESTS];`.
- Inside `setup()` (after `ReceiveTravelTimes` and before `StopWiFi()`), add:
```cpp
        if (HasConfiguredNina()) {
          bool RxNina = false;
          for (byte ninaAttempt = 1; !RxNina && ninaAttempt <= 2; ninaAttempt++) {
            RxNina = ReceiveNinaWarnings(true);
          }
          if (!RxNina) AddToEventLog("*** Failed to Rx NINA warnings ***");
        }
```

- [ ] **Step 4: Verify compilation with PlatformIO**

Run: `/Users/mb/.local/bin/pio run`
Expected: `========================= [SUCCESS] Took ... =========================` with exit code 0.

- [ ] **Step 5: Verify live NINA API endpoint shape**

Run: `curl -s https://warnung.bund.de/api31/dashboard/059130000000.json | head -c 500`
Expected: JSON array matching the filtered paths (`sent`, `payload.data.headline`, `payload.data.provider`, `payload.data.severity`, `i18nTitle.de`).

- [ ] **Step 6: Commit Task 1**

```bash
git add src/config.example.h src/nina.h Station.ino
git commit -m "feat(nina): add NINA warning fetch and decoder"
```

---

### Task 2: Localization Strings and Hi-Vis 2×2 Warning Grid UI (`Station.ino`)

**Files:**
- Modify: `src/lang_gr.h:83-92`
- Modify: `src/lang.h:83-92`
- Modify: `Station.ino:95-98` (`version`), `Station.ino:268-281` (`DisplayWeather`), `Station.ino:970-980` (new `DisplayNinaSection` & helpers), `Station.ino:1654-1658` (changelog)

**Interfaces:**
- Consumes: `ninaData`, `MAX_NINA_WARNINGS`, `HasConfiguredNina()` from `src/nina.h`; `DrawInfoCardHeader`, `DrawCenteredText`, `TruncateTextToWidth`, `TXT_NOT_CONF`
- Produces:
  - Localization constants in `src/lang_gr.h` and `src/lang.h`: `TXT_NINA_TITLE`, `TXT_NINA_NO_WARNINGS`, `TXT_NINA_FETCH_ERR`, `TXT_NINA_MORE`, `TXT_NINA_SEV_EXTREME`, `TXT_NINA_SEV_SEVERE`, `TXT_NINA_SEV_MODERATE`, `TXT_NINA_SEV_MINOR`
  - `void DrawNinaWarningIcon(int centerX, int centerY, uint16_t color, uint16_t cutColor)`
  - `void DrawWrappedCardText(int x, int y, int maxWidth, int lineHeight, int maxLines, const String& text)`
  - `void DrawNinaCard(int x, int y, int w, int h, const NinaWarning& warn)`
  - `void DisplayNinaSection(int x, int y, int width, int height)`

- [ ] **Step 1: Add German and English NINA strings to `src/lang_gr.h` and `src/lang.h`**

In `src/lang_gr.h`:
- `TXT_NINA_TITLE = "NINA Warnungen"`
- `TXT_NINA_NO_WARNINGS = "Keine aktuellen Warnungen"`
- `TXT_NINA_FETCH_ERR = "NINA-Abruf fehlgeschlagen"`
- `TXT_NINA_MORE = "weitere"`
- `TXT_NINA_SEV_EXTREME = "EXTREM"`
- `TXT_NINA_SEV_SEVERE = "SCHWER"`
- `TXT_NINA_SEV_MODERATE = "MITTEL"`
- `TXT_NINA_SEV_MINOR = "GERING"`

In `src/lang.h`:
- `TXT_NINA_TITLE = "NINA Warnings"`
- `TXT_NINA_NO_WARNINGS = "No active warnings"`
- `TXT_NINA_FETCH_ERR = "NINA fetch failed"`
- `TXT_NINA_MORE = "more"`
- `TXT_NINA_SEV_EXTREME = "EXTREME"`
- `TXT_NINA_SEV_SEVERE = "SEVERE"`
- `TXT_NINA_SEV_MODERATE = "MODERATE"`
- `TXT_NINA_SEV_MINOR = "MINOR"`

- [ ] **Step 2: Implement hi-vis B/W card helpers and `DisplayNinaSection` in `Station.ino`**

1. `DrawNinaWarningIcon(int centerX, int centerY, uint16_t color, uint16_t cutColor)`:
   Draws a crisp 14×12 px warning triangle centered at `(centerX, centerY)` with a 2px exclamation mark cut out in `cutColor`.
2. `DrawWrappedCardText(int x, int y, int maxWidth, int lineHeight, int maxLines, const String& text)`:
   Greedily wraps words onto up to `maxLines` lines using `u8g2Fonts.getUTF8Width`, breaking oversized single tokens or truncating the final line with `TruncateTextToWidth(..., maxWidth)` so text never overflows the card border.
3. `FormatNinaSeverityLabel(int rank)`: maps `4 -> TXT_NINA_SEV_EXTREME`, `3 -> TXT_NINA_SEV_SEVERE`, `2 -> TXT_NINA_SEV_MODERATE`, `1 -> TXT_NINA_SEV_MINOR`, else `""`.
4. `DrawNinaCard(int x, int y, int w, int h, const NinaWarning& warn)`:
   - Common header geometry (`headerH = 22`, `padX = 7`):
     - If `warn.severityRank >= 3` (`Extreme` / `Severe`): fill entire card `GxEPD_BLACK`, draw 1px inner white inset frame `(x+2, y+2, w-4, h-4)` for `Extreme` (`rank == 4`), draw horizontal white divider at `y + headerH`, set `u8g2Fonts.setForegroundColor(GxEPD_WHITE)` / `setBackgroundColor(GxEPD_BLACK)`, draw white warning triangle + `provider` (left) + severity label (right) in `u8g2_font_helvB08_te`, and draw up to 3 wrapped lines of headline in `u8g2_font_helvB10_te` (`lineHeight = 16`, starting `y + headerH + 18`). Always restore `GxEPD_BLACK` foreground / `GxEPD_WHITE` background before returning!
     - If `warn.severityRank < 3` (`Moderate` / `Minor` / `Unknown`): draw 2px black border (`drawRect` + inner `drawRect`), fill solid black `headerH` strip `(x, y, w, headerH)`, draw white icon + `provider` + severity label in `u8g2_font_helvB08_te` (with white foreground/black background, then restore black/white), and draw up to 3 wrapped lines of black headline in `u8g2_font_helvB10_te` (`lineHeight = 16`, starting `y + headerH + 18`).
5. `DisplayNinaSection(int x, int y, int width, int height)`:
   - If `!HasConfiguredNina()`: `DrawInfoCardHeader(x, y, width, height, TXT_NINA_TITLE)` and print `TXT_NOT_CONF` centered/padded inside.
   - Else if `!ninaData.valid`: `DrawInfoCardHeader(x, y, width, height, TXT_NINA_TITLE)` and print `TXT_NINA_FETCH_ERR`.
   - Else if `ninaData.totalCount == 0`: `DrawInfoCardHeader(x, y, width, height, TXT_NINA_TITLE)` and `DrawCenteredText(x + width / 2, y + 114, TXT_NINA_NO_WARNINGS)` in `u8g2_font_helvB12_te`.
   - Else render 2×2 grid (`gap = 8`, `cardW = (width - gap) / 2` -> `173`, `cardH = (height - gap) / 2` -> `86`):
     - Loop `i = 0 .. min(ninaData.totalCount, MAX_NINA_WARNINGS) - 1`: `col = i % 2`, `row = i / 2`, `cx = x + col * (cardW + gap)`, `cy = y + row * (cardH + gap)`, call `DrawNinaCard(cx, cy, cardW, cardH, ninaData.warnings[i])`.
     - If `ninaData.totalCount > MAX_NINA_WARNINGS`: set `u8g2Fonts.setFont(u8g2_font_helvB10_te)` and `DrawCenteredText(x + width / 2, y + height + 18, "+" + String(ninaData.totalCount - MAX_NINA_WARNINGS) + " " + TXT_NINA_MORE)` (at `y = 648`, safely above Calendar's `"Termine"` header).

- [ ] **Step 3: Call `DisplayNinaSection` in `DisplayWeather()` and bump version**

In `DisplayWeather()` right before `DisplayForecastSection(372, 450);`:
```cpp
    DisplayNinaSection(5, 450, 355, 180);
```
Bump `String version = "1.16";` and append `Version 1.16` changelog entry at the bottom of `Station.ino`.

- [ ] **Step 4: Compile check with PlatformIO**

Run: `/Users/mb/.local/bin/pio run`
Expected: `========================= [SUCCESS] Took ... =========================` with exit code 0 and zero compiler warnings/errors in our added files.

- [ ] **Step 5: Commit Task 2**

```bash
git add src/lang_gr.h src/lang.h Station.ino
git commit -m "feat(ui): render NINA warnings as 2x2 hi-vis grid"
```

---

### Task 3: Document NINA API Integration & Regional Keys in `README.md`

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: `ninaApiArs` config name and `src/nina.h` behavior from Tasks 1–2
- Produces: README documentation for NINA API, `ninaApiArs`, and regional key lookup link

- [ ] **Step 1: Update `README.md`**

1. In Quick Start (`Edit src/config.h`) or a dedicated `## NINA Warnings` section (matching `## Google Calendar Integration`), document:
   - The implemented NINA public API endpoint: `https://warnung.bund.de/api31/dashboard/{ARS}.json` (OpenAPI spec: `https://nina.api.bund.dev/openapi.yaml`).
   - Config parameter `ninaApiArs` in `src/config.h` (12-digit Kreis-level Amtlicher Regionalschlüssel, e.g. `"059130000000"`).
   - How regional keys can be obtained from `https://www.dcat-ap.de/def/politicalGeocoding/regionalKey/`.
   - Brief behavior summary: displays up to 4 active warnings prioritized by severity (`Extreme` > `Severe` > `Moderate` > `Minor`) as hi-vis B/W cards to the left of the 3-hour forecast, with `+N weitere` overflow when more than 4 warnings are active.
2. Add `src/nina.h` to `## Project Structure`.

- [ ] **Step 2: Run final compile & git status check**

Run: `/Users/mb/.local/bin/pio run && git status`
Expected: Build succeeds; `platformio.ini` and `src/config.h` remain untracked/unstaged as expected, and `README.md` is ready to commit.

- [ ] **Step 3: Commit Task 3**

```bash
git add README.md
git commit -m "docs(readme): document NINA warnings API and ARS lookup"
```
