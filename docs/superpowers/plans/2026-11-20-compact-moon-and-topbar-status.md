# Compact Moon Card & Top-Bar Status Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the Wi-Fi and battery status right-aligned into the top bar, and compress the astronomy card into a 3-column horizontal layout (sunrise icon + time, sunset icon + time, smaller moon symbol + smaller phase text) in the freed space between Date/Time and Fuel Prices.

**Architecture:** All changes live in `Station.ino`. We replace the standalone `DisplayStatusSection` card at `(5, 330)` with a compact single-line `DrawTopBarStatus` helper inside `y = 0..40`, then resize and reposition `DisplayAstronomySection` into `(5, 332, astroW, 106)` directly below Date/Time and left of `DisplayFuelSection(cardsX, 238, ...)`. Vector icon helpers (`DrawSunriseIcon`, `DrawSunsetIcon`) replace `TXT_SUNRISE`/`TXT_SUNSET` strings, and `DrawMoon` gains a configurable `diameter` parameter with center-aligned geometry.

**Tech Stack:** ESP32, Arduino framework, PlatformIO (`pio run`), `GxEPD2_BW` (1304×984 1-bit e-Paper), `U8g2_for_Adafruit_GFX`.

**Spec:** User-directed design from brainstorming session (2026-11-20).

## Global Constraints

- Display resolution is `1304×984` (`SCREEN_WIDTH = 1304`, `SCREEN_HEIGHT = 984`), monochrome (`GxEPD_BLACK` / `GxEPD_WHITE`).
- `Station.ino` is the entry point (`src_dir = "."`); preserve existing `lang.h` / `lang_gr.h` compatibility (do not remove constants used elsewhere).
- Top bar divider is at `y = 40`; Date/Time text sits at `x = 12, y = 265..317`; Fuel Prices card sits at `x = cardsX, y = 238..438` where `cardsX = max(372, 5 + statusW + 14)`.
- Verification command: `/Users/mb/.local/bin/pio run` (compile check; no unit test suite exists in repo; `upload_port` requires hardware).
- Bump `String version` (`"1.13"` -> `"1.14"`) and append `Version 1.14` changelog entry at bottom of `Station.ino`.

## Review Focus

- **Top-bar vertical fit (`y = 0..40`):** Single-line Wi-Fi bars, RSSI dBm, battery icon, percentage, and voltage must stay strictly above the `y = 40` divider line and right of the centered `title + " - " + City`.
- **Battery unavailable (`voltage <= 1.0f`):** Preserve existing `if (voltage > 1)` guard so USB-powered boards without a battery reading still render Wi-Fi status cleanly right-aligned without a phantom battery icon.
- **Long localized moon phase names (`"Zunehmende Sichel"`, `"Zunehmender Mond"`):** Column 3 must either fit `u8g2_font_helvB10_tf` within its width or wrap cleanly at a space onto two centered lines without clipping the card border.
- **Moon circle centering (`DrawMoon`):** Existing `DrawMoon` draws its circle centered at `(x + diameter, y + diameter)` rather than `(x + diameter/2, y + diameter/2)`; offsetting `x = centerX - diameter`, `y = centerY - diameter` must place the circle centered in Column 3.
- **Long date strings widening `cardsX`:** Keeping `astroW = min(365, cardsX - 17)` guarantees the astronomy card never touches `DisplayFuelSection` even if `Date_str` pushes `cardsX` past `372`.

---

### Task 1: Move Wi-Fi and Battery Status into the Top Bar

**Files:**
- Modify: `Station.ino:255-296` (`DisplayWeather`, `DisplayGeneralInfoSection`)
- Modify: `Station.ino:973-1061` (`DisplayStatusSection`, `DrawRSSI`, `DrawBattery`)

**Interfaces:**
- Produces: `void DrawTopBarStatus(int rightX, int baselineY, int rssi)`
- Consumes: `DrawRSSI` / `DrawBattery` logic (refactored or replaced by compact top-bar rendering), `wifi_signal`, `analogRead(35)`

- [ ] **Step 1: Replace `DisplayStatusSection` with compact single-line `DrawTopBarStatus(int rightX, int baselineY, int rssi)` in `Station.ino`**

Implement compact right-aligned status rendering inside the 40 px top bar (`baselineY = 27`, matching `drawString(..., 20, ..., LEFT)`):
1. Check `float voltage = analogRead(35) / 4096.0 * 7.46;` and compute `percentage` with the existing 4th-degree polynomial clamped to `[0, 100]` when `voltage > 1.0f`.
2. When `voltage > 1.0f`, render battery right-aligned up to `rightX`:
   - Battery label `"100% 4.15V"` (using `String(percentage) + "% " + String(voltage, 2) + "V"`) in `u8g2_font_helvB12_tf` right-aligned at `rightX`, `baselineY`.
   - Compact battery icon (`26×12` outline at `y = baselineY - 11`, `3×6` right nub at `y = baselineY - 8`, proportional black fill inside) positioned `6px` to the left of the battery label.
   - Advance `cursorRightX` to `batX - 16` (16 px gap before Wi-Fi).
3. When `voltage <= 1.0f`, set `cursorRightX = rightX`.
4. Render Wi-Fi RSSI right-aligned up to `cursorRightX`:
   - RSSI label `String(rssi) + "dBm"` in `u8g2_font_helvB12_tf` right-aligned at `cursorRightX`, `baselineY`.
   - Compact 5-bar signal meter (`4px` wide bars spaced `6px` apart, heights `4, 8, 12, 16, 20` px resting on `baselineY`, using the existing `-100/-80/-60/-40/-20` dBm thresholds) placed `6px` left of the RSSI text.
5. Remove the now-unused `DisplayStatusSection`, `DrawRSSI`, and `DrawBattery` functions (keep clean, zero dead code).

- [ ] **Step 2: Wire `DrawTopBarStatus` into `DisplayGeneralInfoSection()` and remove `DisplayStatusSection` call from `DisplayWeather()`**

In `DisplayGeneralInfoSection()`, call `DrawTopBarStatus(SCREEN_WIDTH - 12, 27, wifi_signal);` before `display.drawLine(5, 40, SCREEN_WIDTH - 8, 40, GxEPD_BLACK);`.
In `DisplayWeather()`, remove the `DisplayStatusSection(5, 330, statusW, wifi_signal);` call, while keeping `statusW` and `cardsX = max(372, 5 + statusW + 14)` intact so Date/Time width still protects `DisplayFuelSection`.

- [ ] **Step 3: Compile check with PlatformIO**

Run: `/Users/mb/.local/bin/pio run`
Expected: `========================= [SUCCESS] Took ... =========================` with exit code 0 and zero compiler errors.

- [ ] **Step 4: Commit Task 1**

```bash
git add Station.ino
git commit -m "feat(ui): move Wi-Fi and battery status to top bar"
```

---

### Task 2: Compress Astronomy Card Horizontally with Sunrise/Sunset Icons and Smaller Moon

**Files:**
- Modify: `Station.ino:260-282` (`DisplayWeather`)
- Modify: `Station.ino:522-571` (`DisplayAstronomySection`, `DrawMoon`)

**Interfaces:**
- Produces:
  - `void DrawSunriseIcon(int centerX, int centerY)`
  - `void DrawSunsetIcon(int centerX, int centerY)`
  - `void DrawMoon(int x, int y, int diameter, int dd, int mm, int yy, String hemisphere)`
  - `void DisplayAstronomySection(int x, int y, int width, int height)`
- Consumes: `WxConditions[0].Sunrise`, `WxConditions[0].Sunset`, `WxConditions[0].Timezone`, `ConvertUnixTime`, `MoonPhase`, `NormalizedMoonPhase`, `Hemisphere`, `DrawCenteredText`

- [ ] **Step 1: Add `DrawSunriseIcon(int centerX, int centerY)` and `DrawSunsetIcon(int centerX, int centerY)`**

Create two crisp monochrome vector icons (~`36×28` px) using Adafruit GFX primitives (`drawLine`, `fillRect`, `drawCircle`, `fillCircle`, `fillTriangle`):
- Shared horizon helper or clean geometry: a `32px` wide, `2px` thick horizon line at `horizonY = centerY + 6`, with a white mask below `horizonY` so the rising/setting sun semicircle sits cleanly on the horizon, short radiating rays above the horizon, and a `10px` upward arrow (`DrawSunriseIcon`) or downward arrow (`DrawSunsetIcon`) centered above the sun dome (`centerY - 12` to `centerY - 4`) so sunrise and sunset are unmistakable at a glance without text labels.

- [ ] **Step 2: Update `DrawMoon` to accept `int diameter` and center cleanly**

Update `DrawMoon(int x, int y, int diameter, int dd, int mm, int yy, String hemisphere)` replacing the hardcoded `const int diameter = 75;` with the `diameter` parameter (`40` when called from the compact card). Keep the exact phase illumination math unchanged.

- [ ] **Step 3: Rewrite `DisplayAstronomySection(int x, int y, int width, int height)` as a 3-column horizontal card**

1. Draw outer frame `display.drawRect(x, y, width, height, GxEPD_BLACK);`.
2. Partition `width` into 3 horizontal columns with subtle full-height (or padded) vertical dividers at `col1W = width * 27 / 100`, `col2W = width * 27 / 100`, `col3W = width - col1W - col2W`:
   - Column 1 center: `col1X = x + col1W / 2` -> `DrawSunriseIcon(col1X, y + 36)` and `u8g2_font_helvB14_tf` `DrawCenteredText(col1X, y + 86, sunriseTime)`.
   - Column 2 center: `col2X = x + col1W + col2W / 2` -> `DrawSunsetIcon(col2X, y + 36)` and `u8g2_font_helvB14_tf` `DrawCenteredText(col2X, y + 86, sunsetTime)`.
   - Column 3 center: `col3X = x + col1W + col2W + col3W / 2`:
     - Compute `diameter = 40`; call `DrawMoon(col3X - diameter, (y + 34) - diameter, diameter, day_utc, month_utc, year_utc, Hemisphere);` so the 40 px moon disc is centered at `(col3X, y + 34)` (`y + 14..y + 54`).
     - Set `u8g2Fonts.setFont(u8g2_font_helvB10_tf)`. Measure `MoonPhase(...)`: if `getUTF8Width <= col3W - 8`, render single line centered at `(col3X, y + 84)`; otherwise split at the last space that fits within `col3W - 8` and render two centered lines at `y + 76` and `y + 94`.

- [ ] **Step 4: Update `DisplayAstronomySection` call in `DisplayWeather()`**

Compute `int astroW = min(365, cardsX - 17);` (gives `355` when `cardsX = 372`, `12px` left of Fuel Prices and flush with Forecast at `x = 372`) and call `DisplayAstronomySection(5, 332, astroW, 106);` (spanning `y = 332..438`, bottom-aligned with `DisplayFuelSection` and `DisplayTravelSection`).

- [ ] **Step 5: Compile check with PlatformIO**

Run: `/Users/mb/.local/bin/pio run`
Expected: `========================= [SUCCESS] Took ... =========================` with exit code 0.

- [ ] **Step 6: Commit Task 2**

```bash
git add Station.ino
git commit -m "feat(ui): compress astronomy card into horizontal 3-part layout"
```

---

### Task 3: Version Bump and Changelog

**Files:**
- Modify: `Station.ino:97` (`String version`)
- Modify: `Station.ino:1535-1539` (changelog)

- [ ] **Step 1: Bump `version` to `"1.14"` and document changes in changelog**

Update `String version = "1.14";` and append a `Version 1.14` block at the bottom of `Station.ino` summarizing the top-bar Wi-Fi/battery relocation and compact horizontal astronomy card.

- [ ] **Step 2: Final compile check with PlatformIO**

Run: `/Users/mb/.local/bin/pio run`
Expected: `========================= [SUCCESS] Took ... =========================` with exit code 0.

- [ ] **Step 3: Commit Task 3**

```bash
git add Station.ino
git commit -m "chore(version): bump version to 1.14"
```
