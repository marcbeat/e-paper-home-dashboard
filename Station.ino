/* ESP32 Weather Display using an EPD 12.48" 1304x984 Display, obtains data from Open Weather Map, decodes and then displays it.
  ####################################################################################################################################
  This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.

  Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
  1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
  2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
  3. You may not, except with my express written permission, distribute or commercially exploit the content.
  4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

  The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
  software use is visible to an end-user.

  THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY
  OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  See more at http://www.dsbird.org.uk

  Display: Waveshare 12.48" e-Paper (B/W), 1304x984, GxEPD2_1248 driver (GDEW1248T3). No library patching needed.
   The panel needs a 3.3V data level and a stable 5V supply (peak current during refresh is high).
*/

#include "config.h"                   // Copy src/config.example.h to src/config.h and enter your OWM API key and WiFi credentials
#include <ArduinoJson.h>              // https://github.com/bblanchon/ArduinoJson needs version v6 or above
#include <WiFi.h>                     // Built-in
#include <HTTPClient.h>               // Built-in
#include <ESPmDNS.h>
#include "time.h"                     // Built-in
#include <SPI.h>                      // Built-in 
#define  ENABLE_GxEPD2_GFX 1          // enable GxEPD2_GFX base class
#include <GxEPD2_BW.h>
//#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>

/* Private includes *
    to prevent not found issues make the files available in the library folder of the Arduino IDE:
    ln -s ~/Arduino/ESP32-e-Paper-Weather-Display-master/src ~/Arduino/libraries/G6EJD-Lib
*/
#include "epaper_fonts.h"
// #include "lang.h"                     // Localisation (English)
//#include "lang_cz.h"                  // Localisation (Czech)
//#include "lang_fr.h"                  // Localisation (French)
#include "lang_gr.h"                  // Localisation (German)
//#include "lang_it.h"                  // Localisation (Italian)
//#include "lang_nl.h"
//#include "lang_pl.h"                  // Localisation (Polish)

#undef BUILTIN_LED                      // GPIO 5 is RST2 of the 12.48" panel, so don't touch it as an LED

#define SCREEN_WIDTH  1304             // Set for landscape mode (= GxEPD2_1248::WIDTH)
#define SCREEN_HEIGHT 984              // (= GxEPD2_1248::HEIGHT)

enum alignment {LEFT, RIGHT, CENTER};

// Connections for the Waveshare 12.48" e-Paper (B/W) on the Waveshare ESP32 Driver Board
// (the 12.48" panel has 4 controllers: M1, S1, M2, S2)
static const uint8_t EPD_SCK     = 13;
static const uint8_t EPD_MISO    = 12; // not used by the panel, but required by the constructor
static const uint8_t EPD_MOSI    = 14;
static const uint8_t EPD_CS_M1   = 23;
static const uint8_t EPD_CS_S1   = 22;
static const uint8_t EPD_CS_M2   = 16;
static const uint8_t EPD_CS_S2   = 19;
static const uint8_t EPD_DC1     = 25;
static const uint8_t EPD_DC2     = 17;
static const uint8_t EPD_RST1    = 33;
static const uint8_t EPD_RST2    = 5;
static const uint8_t EPD_BUSY_M1 = 32;
static const uint8_t EPD_BUSY_S1 = 26;
static const uint8_t EPD_BUSY_M2 = 18;
static const uint8_t EPD_BUSY_S2 = 4;

// 1304x984 / 4 pages = ~40kB frame buffer
GxEPD2_BW<GxEPD2_1248, GxEPD2_1248::HEIGHT / 4> display(GxEPD2_1248(EPD_SCK, EPD_MISO, EPD_MOSI,
                                                                    EPD_CS_M1, EPD_CS_S1, EPD_CS_M2, EPD_CS_S2,
                                                                    EPD_DC1, EPD_DC2, EPD_RST1, EPD_RST2,
                                                                    EPD_BUSY_M1, EPD_BUSY_S1, EPD_BUSY_M2, EPD_BUSY_S2));
// use GxEPD_BLACK or GxEPD_WHITE

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;  // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
// Using fonts:
// u8g2_font_helvB08_tf
// u8g2_font_helvB10_tf
// u8g2_font_helvB12_tf
// u8g2_font_helvB14_tf
// u8g2_font_helvB18_tf
// u8g2_font_helvB24_tf
// u8g2_font_fub35_tf
// u8g2_font_fub42_tf


//################  VERSION  ##################################################
#ifndef BUILD_GIT_SHA
#define BUILD_GIT_SHA "unknown"
#endif
String version = "1.8";     // Programme version, see change log at end
//################ VARIABLES ##################################################

const uint8_t SmallIcon  = 0;
const uint8_t MediumIcon = 1;
const uint8_t LargeIcon  = 2;
#define Large  28           // For icon drawing, needs to be odd number for best effect
#define Medium 15           // For compact top-row Conditions card icon
#define Small  8            // 6  For icon drawing, needs to be odd number for best effect
const byte MaxEvents = 10;  // For event reporting, the maximum that can be recorded
String     Time_str = "--:--:--";
String     Date_str = "-- -- ----";
String     EventMessage[MaxEvents]; // strings for Time, Date and Error reporting
const byte EventThreshold = 2; // Change to 1 to view all messages on e-paper screen
int        wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0, EventCnt = 0;

//################ PROGRAM VARIABLES and OBJECTS ################
bool ReceiveOneCallWeather(WiFiClient& json, bool print);
bool DecodeOneCallWeather(WiFiClient& json, bool print);

#define max_readings 24

#include "common.h"
#include "calendar.h"
#include <rom/rtc.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

float temperature_readings[max_readings] = {0};
float rain_readings[max_readings]        = {0};
float snow_readings[max_readings]        = {0};

CalendarEvent cal1Events[MAX_CALENDAR_EVENTS];
CalendarEvent cal2Events[MAX_CALENDAR_EVENTS];
int cal1EventCount = 0;
int cal2EventCount = 0;

long SleepDuration = 30; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int  WakeupTime    = 7;  // Don't wakeup until after 07:00 to save battery power
int  SleepTime     = 23; // Sleep after (23+1) 00:00 to save battery power
long StartTime = 0, SleepTimer = 0;
long Delta         = 12; // correction factor to compensate the ESP32 speed
// Prevents display at xx:59:yy and then xx:00:yy (one minute later) to save power
String LocalIP;

//#########################################################################################
void setup() {
  StartTime = millis();
  DisableBrownOutDetector();
  VerboseRecordOfResetReason(rtc_get_reset_reason(0)); // 0 means CPU0 (Main core)
  delay(500); // Allow the PSU to stabilise

  Serial.begin(115200);

  // Early initilisation of the display to be able to display the EventLog in case of a WiFi problem
  InitialiseDisplay(); // Give screen time to initialise by getting weather data!

  if (StartWiFi() == WL_CONNECTED && SetupTime() == true) {
    if ((CurrentHour >= WakeupTime && CurrentHour <= SleepTime)) {
      //khl moved to above;     InitialiseDisplay(); // Give screen time to initialise by getting weather data!
      byte Attempts = 1;
      bool RxWeather = false;
      WiFiClient client;   // wifi client object
      while ((RxWeather == false) && Attempts <= 2) { // Try up-to 2 time for Weather and Forecast data
        if (RxWeather  == false) RxWeather  = ReceiveOneCallWeather(client, true);
        Attempts++;
      }
      if (RxWeather) { // Only if received both Weather or Forecast proceed
        if (strlen(calendarBridgeUrl) > 0) {
          bool RxCalendar = false;
          for (byte calAttempt = 1; !RxCalendar && calAttempt <= 2; calAttempt++) {
            RxCalendar = ReceiveTodayCalendars(true);
          }
          if (!RxCalendar) AddToEventLog("*** Failed to Rx Calendar data ***");
        }
        StopWiFi(); // Reduces power consumption
        DisplayWeather();
      }
      else
      {
        if (!RxWeather)  AddToEventLog("*** Failed to Rx Weather data ***");
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("StopWiFi to reduce power consumption");
    StopWiFi(); // Reduces power consumption
  }

  if ((CurrentHour == SleepTime)) {
    Serial.println("Sleeping for 3600 * WakeupTime seconds....");
    SleepTimer = WakeupTime * 3600;
  }
  else {
    SleepTimer = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec - Delta)); //Some ESP32 are too fast to maintain accurate time
  }
  BeginSleep();
}
//#########################################################################################
void loop() { // this will never run!
}
//#########################################################################################
void BeginSleep() {

  // The window size is set accordingly to the number of events
  // the report event section:  display.setPartialWindow(wx, wy, tbw, tbh)
  AddToEventLog("*** Entering Sleep *** at " + Time_str + " on " + Date_str);
  if (EventCnt > EventThreshold) {
    display.firstPage();
    do {
    ReportEvent(EventMessage);
    } while (display.nextPage());
  }
  Serial.printf("Available heap : %d\n", ESP.getFreeHeap());
  Serial.printf("Min free heap:   %d\n", ESP.getMinFreeHeap());
  Serial.printf("Largest block of heap: %d\n", ESP.getMaxAllocHeap());
  delay(500);
  //  display.powerOff();
  display.hibernate();     // Hibernate instead of powerOff

  esp_sleep_enable_timer_wakeup(SleepTimer * 1000000LL);

#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Entering " + String(SleepTimer) + "-secs of sleep time");
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();      // Sleep for e.g. 30 minutes
}
//#########################################################################################
void DisableBrownOutDetector() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
}
//#########################################################################################
void DisplayWeather() {                          // 12.48" e-paper display is 1304x984 resolution
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  int dateW = u8g2Fonts.getUTF8Width(Date_str.c_str());
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  int timeW = u8g2Fonts.getUTF8Width((String(TXT_UPDATED) + Time_str).c_str());
  int statusX = max(315, 12 + max(dateW, timeW) + 32);
  if (statusX > SCREEN_WIDTH - 212) statusX = SCREEN_WIDTH - 212;

  display.firstPage();
  do
  {
    DisplayGeneralInfoSection();                 // Top line and date/time below top row
    DisplayDisplayWindSection(5, 40, WxConditions[0].Winddir, WxConditions[0].Windspeed, 214, 190);
    DisplayAstronomySection(5, 430);             // Astronomy section Sun rise/set, Moon phase and Moon icon
    DisplayMainWeatherSection(221, 40);          // Top row cards for conditions, temperature, pressure, precipitation
    DisplayStatusSection(statusX, 242, wifi_signal); // Wi-Fi signal strength and Battery voltage to the right of date/time
    DisplayForecastSection(372, 450);            // 3hr forecast boxes and right-aligned 3-day charts
    DisplayCalendarSection(5, 674, 635, 290);    // Bottom-left today's events for cal1 and cal2
  }
  while (display.nextPage());
}
//#########################################################################################
void DisplayGeneralInfoSection() {
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(12, 20, "Version " + version + " (" + BUILD_GIT_SHA + ")", LEFT); // Programme version
  drawString(SCREEN_WIDTH / 2, 20, City, CENTER);
  // Uncomment the next line if the display of IP- and MAC-Adddress is wanted
  //drawString(SCREEN_WIDTH - 150, 20, "IP=" + LocalIP + ",  MAC=" + WiFi.macAddress() ,RIGHT);
  display.drawLine(5, 40, SCREEN_WIDTH - 8, 40, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  drawString(12, 265, Date_str, LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  drawString(12, 305, TXT_UPDATED + Time_str, LEFT);
}
//#########################################################################################
void DisplayMainWeatherSection(int x, int y) {  // (x=221, y=40)
  DisplayConditionsCard(x, y, 427, 190);
  DisplayTemperatureSection(725, y, 214, 190);
  DisplayPressureSection(922, y, 214, 190, WxConditions[0].Pressure, WxConditions[0].Trend);
  DisplayPrecipitationSection(1130, y, 214, 190);
  // Header divider lines inside Temperature, Pressure, and Precipitation cards
  display.drawLine(650,  y + 40, 863,  y + 40, GxEPD_BLACK);
  display.drawLine(866,  y + 40, 1079, y + 40, GxEPD_BLACK);
  display.drawLine(1082, y + 40, 1295, y + 40, GxEPD_BLACK);
}
//#########################################################################################
void DrawTornadoIcon(int x, int y) {
  const int8_t dx[7] = {  2, -1, -3, -2,  1,  4,  6 };
  const uint8_t w[7] = { 26, 22, 18, 14, 10,  7,  4 };
  const int8_t dy[7] = {-12, -8, -4,  0,  4,  8, 12 };
  for (int i = 0; i < 7; i++) {
    int bx = x + dx[i] - w[i] / 2;
    int by = y + dy[i];
    display.fillRoundRect(bx, by, w[i], 3, 1, GxEPD_BLACK);
  }
  display.fillCircle(x + 7, y + 15, 1, GxEPD_BLACK);
}
//#########################################################################################
void DrawCompassIcon(int x, int y, float angle) {
  const int r = 14;
  display.drawCircle(x, y, r, GxEPD_BLACK);
  display.drawCircle(x, y, r - 1, GxEPD_BLACK);
  display.drawLine(x, y - r, x, y - r + 3, GxEPD_BLACK);
  display.drawLine(x + r - 3, y, x + r, y, GxEPD_BLACK);
  display.drawLine(x, y + r - 3, x, y + r, GxEPD_BLACK);
  display.drawLine(x - r, y, x - r + 3, y, GxEPD_BLACK);
  float rad = (angle - 90.0f) * PI / 180.0f;
  float cosA = cos(rad), sinA = sin(rad);
  int tipX = round(x + 10.0f * cosA);
  int tipY = round(y + 10.0f * sinA);
  int tailX = round(x - 10.0f * cosA);
  int tailY = round(y - 10.0f * sinA);
  int side1X = round(x - 3.5f * sinA);
  int side1Y = round(y + 3.5f * cosA);
  int side2X = round(x + 3.5f * sinA);
  int side2Y = round(y - 3.5f * cosA);
  display.fillTriangle(tipX, tipY, side1X, side1Y, side2X, side2Y, GxEPD_BLACK);
  display.drawTriangle(tailX, tailY, side1X, side1Y, side2X, side2Y, GxEPD_BLACK);
  display.fillCircle(x, y, 1, GxEPD_WHITE);
}
//#########################################################################################
void DisplayDisplayWindSection(int x, int y, float angle, float windspeed, int wwidth, int wdepth) {
  display.drawRect(x, y, wwidth, wdepth, GxEPD_BLACK);
  display.drawLine(x, y + 40, x + wwidth - 1, y + 40, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + wwidth / 2 - 9, y + 20, TXT_WIND_SPEED_DIRECTION, CENTER);
  DrawTornadoIcon(x + 30, y + 90);
  DrawCompassIcon(x + 30, y + 145, angle);
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  drawString(x + 56, y + 91, String(windspeed, 1) + " " + (Units == "M" ? "m/s" : "mph"), LEFT);
  drawString(x + 56, y + 146, WindDegToDirection(angle) + " (" + String(angle, 0) + "°)", LEFT);
}
//#########################################################################################
String WindDegToDirection(float winddirection) {
  int dir = int((winddirection / 22.5) + 0.5);
  String Ord_direction[16] = {TXT_N, TXT_NNE, TXT_NE, TXT_ENE, TXT_E, TXT_ESE, TXT_SE, TXT_SSE, TXT_S, TXT_SSW, TXT_SW, TXT_WSW, TXT_W, TXT_WNW, TXT_NW, TXT_NNW};
  return Ord_direction[(dir % 16)];
}
//#########################################################################################
void DisplayTemperatureSection(int x, int y, int twidth, int tdepth) {
  int dx = (twidth - 180) / 2, dy = (tdepth - 170) / 2; // content was designed for a 180x170 box
  display.drawRect(x - 75, y, twidth, tdepth, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + dx, y + 20, TXT_TEMPERATURES, CENTER);
  u8g2Fonts.setFont(u8g2_font_fub35_tf);
  drawString(x - 30 + dx, y + 100 + dy, String(WxConditions[0].Temperature, 1) + "°", CENTER); // Show current Temperature
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  drawString(x + 73 + dx, y + 100 + dy, Units == "M" ? "C" : "F", LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);                                       
  drawString(x + 10 + dx, y + 140 + dy, String(WxConditions[0].High, 0) + "° | " + String(WxConditions[0].Low, 0) + "°", CENTER); // Show forecast high and Low
}
//#########################################################################################
String BuildConditionsDescription() {
  String desc;
  if (Language == "DE") {
    desc = WxConditions[0].Forecast0;
  } else {
    desc = WxConditions[0].Main0;
    if (WxConditions[0].Forecast0 != "") desc += " (" + WxConditions[0].Forecast0;
  }
  if (WxConditions[0].Forecast1 != "") desc += ", " + WxConditions[0].Forecast1;
  if (WxConditions[0].Forecast2 != "") desc += ", " + WxConditions[0].Forecast2;
  if (desc.indexOf("(") > 0) desc += ")";
  return (Language == "DE") ? desc : TitleCase(desc);
}
//#########################################################################################
void DrawWrappedConditionText(int x, int y, int maxWidth, const String& text) {
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  if (u8g2Fonts.getUTF8Width(text.c_str()) <= maxWidth) {
    u8g2Fonts.setCursor(x, y);
    u8g2Fonts.print(text);
    return;
  }
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  if (u8g2Fonts.getUTF8Width(text.c_str()) <= maxWidth) {
    u8g2Fonts.setCursor(x, y - 2);
    u8g2Fonts.print(text);
    return;
  }
  // Split at the last space that fits within maxWidth across up to two lines
  int splitPos = -1;
  for (int i = 0; i < (int)text.length(); i++) {
    if (text[i] == ' ') {
      if (u8g2Fonts.getUTF8Width(text.substring(0, i).c_str()) <= maxWidth) splitPos = i;
      else break;
    }
  }
  if (splitPos <= 0) {
    u8g2Fonts.setCursor(x, y - 4);
    u8g2Fonts.print(text);
    return;
  }
  String line1 = text.substring(0, splitPos);
  String line2 = text.substring(splitPos + 1);
  line2.trim();
  while (line2.length() > 3 && u8g2Fonts.getUTF8Width(line2.c_str()) > maxWidth) {
    line2.remove(line2.length() - 1);
  }
  u8g2Fonts.setCursor(x, y - 4);
  u8g2Fonts.print(line1);
  u8g2Fonts.setCursor(x, y + 20);
  u8g2Fonts.print(line2);
}
//#########################################################################################
void DisplayConditionsCard(int x, int y, int cwidth, int cdepth) {
  display.drawRect(x, y, cwidth, cdepth, GxEPD_BLACK);
  display.drawLine(x, y + 40, x + cwidth - 1, y + 40, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  int titleW = u8g2Fonts.getUTF8Width(TXT_CONDITIONS.c_str());
  u8g2Fonts.setCursor(x + (cwidth - titleW) / 2, y + 28);
  u8g2Fonts.print(TXT_CONDITIONS);

  DisplayConditionsSection(x + 78, y + 118, WxConditions[0].Icon, MediumIcon);

  const int textX = x + 166;
  const int maxTextW = cwidth - 178;
  DrawWrappedConditionText(textX, y + 76, maxTextW, BuildConditionsDescription());

  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  u8g2Fonts.setCursor(textX, y + 124);
  u8g2Fonts.print(String(WxConditions[0].Humidity, 0) + "%");
  int humX = u8g2Fonts.getCursorX();
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  u8g2Fonts.setCursor(humX + 6, y + 124);
  u8g2Fonts.print("RH");

  if (WxConditions[0].Visibility > 0) Visibility(textX + 14, y + 162, String(WxConditions[0].Visibility) + "M");
  if (WxConditions[0].Cloudcover > 0) CloudCover(textX + 150, y + 162, WxConditions[0].Cloudcover);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
}
//#########################################################################################
void DisplayForecastWeather(int x, int y, int index) {
  int fwidth = 115;
  x = x + fwidth * index;
  display.drawRect(x, y, fwidth - 2, 180, GxEPD_BLACK);
  display.drawLine(x, y + 40, x + fwidth - 3, y + 40, GxEPD_BLACK);
  DisplayConditionsSection(x + fwidth / 2, y + 105, WxForecast[index].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + fwidth / 2 - 10, y + 20, String(ConvertUnixTime(WxForecast[index].Dt + WxConditions[0].Timezone).substring(0,5)), CENTER);
  drawString(x + fwidth / 2 + 0, y + 155, String(WxForecast[index].High, 0) + "°/" + String(WxForecast[index].Low, 0) + "°", CENTER);
}
//#########################################################################################
void DisplayPressureSection(int x, int y, int pwidth, int pdepth, float pressure, String slope) {
  int dx = (pwidth - 180) / 2, dy = (pdepth - 170) / 2; // content was designed for a 180x170 box
  display.drawRect(x - 56, y, pwidth, pdepth, GxEPD_BLACK); // pressure outline
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + 8 + dx, y + 20, TXT_PRESSURE, CENTER);
  String slope_direction = TXT_PRESSURE_STEADY;
  if (slope == "+") slope_direction = TXT_PRESSURE_RISING;
  if (slope == "-") slope_direction = TXT_PRESSURE_FALLING;
  u8g2Fonts.setFont(u8g2_font_fub35_tf);
  if (Units == "I") drawString(x - 22 + dx, y + 100 + dy, String(pressure, 2), CENTER); // "Imperial"
  else              drawString(x - 22 + dx, y + 100 + dy, String(pressure, 0), CENTER); // "Metric"
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x - 3 + dx, y + 150 + dy, slope_direction, CENTER);
  display.drawRect(x + 75 + dx, y + 135 + dy, 49, 35, GxEPD_BLACK);
  drawString(x + 90 + dx, y + 150 + dy, (Units == "M" ? "hPa" : "in"), CENTER);
}
//#########################################################################################
void DisplayPrecipitationSection(int x, int y, int pwidth, int pdepth) {
  int dx = (pwidth - 181) / 2, dy = (pdepth - 170) / 2; // content was designed for a 181x170 box
  display.drawRect(x - 48, y, pwidth, pdepth, GxEPD_BLACK); // precipitation outline
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + 10 + dx, y + 20, TXT_PRECIPITATION_SOON, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  if (WxForecast[1].Rainfall >= 0.005) { // Ignore small amounts
    drawString(x - 15 + dx, y + 80 + dy, String(WxForecast[1].Rainfall, 2) + (Units == "M" ? "mm" : "in"), LEFT); // Only display rainfall total today if > 0
    addraindrop(x + 70 + dx, y + 80 + dy, 7);
  }
  if (WxForecast[1].Snowfall >= 0.005)  // Ignore small amounts
    drawString(x - 15 + dx, y + 110 + dy, String(WxForecast[1].Snowfall, 2) + (Units == "M" ? "mm" : "in") + " **", LEFT); // Only display snowfall total today if > 0
  if (WxForecast[1].PoP >= 0.005)       // Ignore small amounts
    drawString(x + 2 + dx, y + 120 + dy, String(WxForecast[1].PoP*100, 0) + "%", LEFT); // Only display pop if > 0
}
//#########################################################################################
void DisplayAstronomySection(int x, int y) {
  const int dy = 35; // box is 180 high instead of 110, centre the content
  display.drawRect(x, y + 20, 365, 180, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + 14, y + 54 + dy, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNRISE, LEFT);
  drawString(x + 14, y + 79 + dy, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNSET, LEFT);
  time_t now = time(NULL);
  struct tm * now_utc  = gmtime(&now);
  const int day_utc = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc = now_utc->tm_year + 1900;
  drawString(x + 14, y + 104 + dy, MoonPhase(day_utc, month_utc, year_utc, Hemisphere), LEFT);
  DrawMoon(x + 225, y + 5 + dy, day_utc, month_utc, year_utc, Hemisphere);
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  const int diameter = 75;
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "south") Phase = 1 - Phase;
  // Draw dark part of moon
  display.fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++) {
    double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);
    // Determine the edges of the lighted part of the moon
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5) {
      Xpos1 = -Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    display.drawLine(pW1x, pW1y, pW2x, pW2y, GxEPD_WHITE);
    display.drawLine(pW3x, pW3y, pW4x, pW4y, GxEPD_WHITE);
  }
  display.drawCircle(x + diameter - 1, y + diameter, diameter / 2, GxEPD_BLACK);
}
//#########################################################################################
String MoonPhase(int d, int m, int y, String hemisphere) {
  int c, e;
  double jd;
  int b;
  if (m < 3) {
    y--;
    m += 12;
  }
  ++m;
  c   = 365.25 * y;
  e   = 30.6  * m;
  jd  = c + e + d - 694039.09;     /* jd is total days elapsed */
  jd /= 29.53059;                        /* divide by the moon cycle (29.53 days) */
  b   = jd;                              /* int(jd) -> b, take integer part of jd */
  jd -= b;                               /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;                /* scale fraction from 0-8 and round by adding 0.5 */
  b   = b & 7;                           /* 0 and 8 are the same phase so modulo 8 for 0 */
  if (hemisphere == "south") b = 7 - b;
  if (b == 0) return TXT_MOON_NEW;              // New;              0%  illuminated
  if (b == 1) return TXT_MOON_WAXING_CRESCENT;  // Waxing crescent; 25%  illuminated
  if (b == 2) return TXT_MOON_FIRST_QUARTER;    // First quarter;   50%  illuminated
  if (b == 3) return TXT_MOON_WAXING_GIBBOUS;   // Waxing gibbous;  75%  illuminated
  if (b == 4) return TXT_MOON_FULL;             // Full;            100% illuminated
  if (b == 5) return TXT_MOON_WANING_GIBBOUS;   // Waning gibbous;  75%  illuminated
  if (b == 6) return TXT_MOON_THIRD_QUARTER;    // Third quarter;   50%  illuminated
  if (b == 7) return TXT_MOON_WANING_CRESCENT;  // Waning crescent; 25%  illuminated
  return "";
}
//#########################################################################################
void DisplayForecastSection(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  int f = 0;
  do {
    DisplayForecastWeather(x, y, f);
    f++;
  } while (f <= 7);
  // Pre-load temporary arrays with data - because C parses by reference
  int r = 0;
  do {
    if (Units == "I") rain_readings[r]     = WxForecast[r].Rainfall * 0.0393701; else rain_readings[r]     = WxForecast[r].Rainfall;
    if (Units == "I") snow_readings[r]     = WxForecast[r].Snowfall * 0.0393701; else snow_readings[r]     = WxForecast[r].Snowfall;
    temperature_readings[r] = WxForecast[r].Temperature;
    r++;
  } while (r < max_readings);

  const int gwidth = 260, gheight = 210;
  const int chartGap = 59;
  const int chart2X = SCREEN_WIDTH - 19 - gwidth;
  const int chart1X = chart2X - gwidth - chartGap;
  const int gy = SCREEN_HEIGHT - gheight - 50;
  const int titleCenterX = (chart1X + chart2X + gwidth) / 2;

  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  DrawCenteredText(titleCenterX, gy - 42, TXT_FORECAST_VALUES);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  // (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  DrawGraph(chart1X, gy, gwidth, gheight, 10, 30, Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature_readings, max_readings, autoscale_on, barchart_off);
  const int Rain_array_size = sizeof(rain_readings) / sizeof(float);
  const int Snow_array_size = sizeof(snow_readings) / sizeof(float);
  if (SumOfPrecip(rain_readings, Rain_array_size) >= SumOfPrecip(snow_readings, Snow_array_size))
    DrawGraph(chart2X, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_readings, Rain_array_size, autoscale_on, barchart_on);
  else DrawGraph(chart2X, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_SNOWFALL_MM : TXT_SNOWFALL_IN, snow_readings, Snow_array_size, autoscale_on, barchart_on);
}
//#############################################################################################
void DrawCenteredText(int centerX, int baselineY, const String& text) {
  int w = u8g2Fonts.getUTF8Width(text.c_str());
  u8g2Fonts.setCursor(centerX - w / 2, baselineY);
  u8g2Fonts.print(text);
}
//#############################################################################################
String TruncateTextToWidth(String text, int maxWidth) {
  if (u8g2Fonts.getUTF8Width(text.c_str()) <= maxWidth) return text;
  while (text.length() > 1 && u8g2Fonts.getUTF8Width((text + "...").c_str()) > maxWidth) {
    // Step back across UTF-8 continuation bytes so multi-byte characters (ä, ö, ü, ß) aren't split
    while (text.length() > 1 && (uint8_t(text[text.length() - 1]) & 0xC0) == 0x80) {
      text.remove(text.length() - 1);
    }
    text.remove(text.length() - 1);
  }
  return text + "...";
}
//#############################################################################################
void DrawCalendarListCard(int x, int y, int w, int h, const char* title, const CalendarEvent events[], int count, bool configured) {
  const int headerH = 36;
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  display.drawLine(x, y + headerH, x + w - 1, y + headerH, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  DrawCenteredText(x + w / 2, y + 25, title);

  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  const int padX = 10;
  const int startY = y + headerH + 24;
  const int rowStep = 26;
  const int maxRows = (h - headerH - 12) / rowStep;

  if (!configured) {
    u8g2Fonts.setCursor(x + padX, startY);
    u8g2Fonts.print(TXT_CAL_NOT_CONF);
    return;
  }
  if (count == 0) {
    u8g2Fonts.setCursor(x + padX, startY);
    u8g2Fonts.print(TXT_NO_EVENTS);
    return;
  }

  const int timeColW = 58;
  const int titleMaxW = w - padX * 2 - timeColW;
  bool overflow = count > maxRows;
  int visibleRows = overflow ? maxRows - 1 : count;

  for (int i = 0; i < visibleRows; i++) {
    int rowY = startY + i * rowStep;
    String timeLabel = events[i].allDay ? TXT_ALL_DAY : events[i].time;
    u8g2Fonts.setCursor(x + padX, rowY);
    u8g2Fonts.print(timeLabel);
    u8g2Fonts.setCursor(x + padX + timeColW, rowY);
    u8g2Fonts.print(TruncateTextToWidth(events[i].title, titleMaxW));
  }
  if (overflow) {
    int rowY = startY + visibleRows * rowStep;
    u8g2Fonts.setCursor(x + padX, rowY);
    u8g2Fonts.print("+" + String(count - visibleRows) + " weitere");
  }
}
//#############################################################################################
void DisplayCalendarSection(int x, int y, int width, int height) {
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  DrawCenteredText(x + width / 2, y + 8, TXT_TODAY_EVENTS);

  const int cardTopOffset = 30;
  const int colGap = 12;
  const int colW = (width - colGap) / 2;
  const int colH = height - cardTopOffset;
  const bool configured = (strlen(calendarBridgeUrl) > 0);

  DrawCalendarListCard(x, y + cardTopOffset, colW, colH, "Piet", cal1Events, cal1EventCount, configured);
  DrawCalendarListCard(x + colW + colGap, y + cardTopOffset, colW, colH, "Nati", cal2Events, cal2EventCount, configured);
}
//#############################################################################################
int iconScale(uint8_t IconSize, int& linesize) {
  if (IconSize == SmallIcon) {
    linesize = 1;
    return Small;
  }
  if (IconSize == MediumIcon) {
    linesize = 2;
    return Medium;
  }
  linesize = 3;
  return Large;
}
//#############################################################################################
void DisplayConditionsSection(int x, int y, String IconName, uint8_t IconSize) {
  Serial.println("Icon name: " + IconName);
  if      (IconName == "01d" || IconName == "01n")  Sunny(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n")  MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n")  Cloudy(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n")  MostlyCloudy(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n")  ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n")  Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n")  Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n")  Snow(x, y, IconSize, IconName);
  else if (IconName == "50d")                       Haze(x, y, IconSize, IconName);
  else if (IconName == "50n")                       Fog(x, y, IconSize, IconName);
  else                                              Nodata(x, y, IconSize, IconName);
  if (IconSize == LargeIcon) {
    display.drawRect(x - 130, y - 230, 270, 410, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(x - 20, y - 210, TXT_CONDITIONS, CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    drawString(x - 55, y + 150, String(WxConditions[0].Humidity, 0) + "%", CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(x + 35, y + 150, "RH", CENTER);
    if (WxConditions[0].Visibility > 0) Visibility(x - 100, y - 160, String(WxConditions[0].Visibility) + "M");
    if (WxConditions[0].Cloudcover > 0) CloudCover(x + 60, y - 160, WxConditions[0].Cloudcover);
  }
}
//#########################################################################################
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;         float y1 = plength;
  float x2 = pwidth / 2;  float y2 = pwidth / 2;
  float x3 = -pwidth / 2; float y3 = pwidth / 2;
  float angle = aangle * PI / 180;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  display.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, GxEPD_BLACK);
}
//#########################################################################################
uint8_t StartWiFi() {
  Serial.print("\r\nConnecting to: "); Serial.println(String(ssid));
  IPAddress dns(8, 8, 8, 8); // Google DNS
  WiFi.disconnect(true);
  delay(500);
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  uint8_t connectionStatus;
  bool AttemptConnection = true;
  while (AttemptConnection) {
    connectionStatus = WiFi.status();
    if (millis() > start + 15000) { // Wait 15-secs maximum
      AttemptConnection = false;
    }
    if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED) {
      AttemptConnection = false;
    }
    delay(1000);
    Serial.printf("WiFi connection status in loop: %d, millis=%d\n", connectionStatus, millis());
  }
  Serial.printf("WiFi connection status: %d\r\n", connectionStatus);
  if (connectionStatus == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    Serial.println("WiFi connected at: " + WiFi.localIP().toString() + ", MAC: " + WiFi.macAddress());
    LocalIP = WiFi.localIP().toString();
    Serial.println(LocalIP);
    if (!MDNS.begin("Weather-1304x984")) {
      Serial.println("Error setting up MDNS responder!");
      while (true);
    }
  }
  else
  {
    // khl add print event message to serial
    AddToEventLog("*** WiFi connection FAILED ***");
    Serial.println(EventMessage[EventCnt]);
      switch (connectionStatus) {
      case  0: AddToEventLog("*** WL_IDLE_STATUS ***"); break;      // temporary status assigned when WiFi.begin() is called and remains active until the number of attempts expires
      case  1: AddToEventLog("*** WL_NO_SSID_AVAIL ***"); break;    // assigned when no SSID or requested SSID are available
      case  2: AddToEventLog("*** WL_SCAN_COMPLETED ***"); break;   // assigned when the scan networks is completed
      case  3: AddToEventLog("*** WL_CONNECTED ***"); break;        // assigned when connected to a WiFi network
      case  4: AddToEventLog("*** WL_CONNECT_FAILED ***"); break;   // assigned when the connection fails for all the attempts
      case  5: AddToEventLog("*** WL_CONNECTION_LOST ***"); break;  // assigned when the connection is lost
      case  6: AddToEventLog("*** WL_DISCONNECTED ***"); break;     // assigned when disconnected from a network;
      default: AddToEventLog("*** WL_UNKNOWN_CONNECTION_STATUS ***"); break;     // khl: Unknown connection status
    }
  }
  // else Serial.println("WiFi connection *** FAILED ***");
  return connectionStatus;
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
//#########################################################################################
void DisplayStatusSection(int x, int y, int rssi) {
  int size = 204;
  display.drawRect(x ,y , size, 108, GxEPD_BLACK);
  display.drawLine(x ,y + 25, x + size, y + 25, GxEPD_BLACK);
  display.drawLine(x + size / 2, y, x + size / 2, y + 25, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(x + 45, y + 10, TXT_WIFI, CENTER);
  drawString(x + 150, y + 10, TXT_POWER, CENTER);
  DrawRSSI(x + 25, y + 75, rssi);
  DrawBattery(x + 130, y + 80);;
}
//#########################################################################################
void DrawRSSI(int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20)  WIFIsignal = 30; //            <-20dbm displays 5-bars
    if (_rssi <= -40)  WIFIsignal = 24; //  -40dbm to  -21dbm displays 4-bars
    if (_rssi <= -60)  WIFIsignal = 18; //  -60dbm to  -41dbm displays 3-bars
    if (_rssi <= -80)  WIFIsignal = 12; //  -80dbm to  -61dbm displays 2-bars
    if (_rssi <= -100) WIFIsignal = 6;  // -100dbm to  -81dbm displays 1-bar
    display.fillRect(x + xpos * 8, y - WIFIsignal, 7, WIFIsignal, GxEPD_BLACK);
    xpos++;
  }
  display.fillRect(x, y - 1, 5, 1, GxEPD_BLACK);
  drawString(x + 6,  y + 10, String(rssi) + "dBm", CENTER);
}
//#########################################################################################
boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "de.pool.ntp.org"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset(); // Set the TZ environment variable
  delay(100);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//#########################################################################################
boolean UpdateLocalTime() {
  struct tm timeinfo;
  char   time_output[30], day_output[30], update_time[30];
  while (!getLocalTime(&timeinfo, 15000)) { // Wait for 15-sec for time to synchronise
    AddToEventLog("*** Failed to obtain time ***");
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");      // Displays: Saturday, June 24 2017 14:05:49
  if (Units == "M") {
    if ((Language == "CZ") || (Language == "DE") || (Language == "PL") || (Language == "NL")) {
      sprintf(day_output, "%s, %02u. %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900); // day_output >> So., 23. Juni 2019 <<
    }
    else
    {
      sprintf(day_output, "%s %02u-%s-%04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    }
    strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo);  // Creates: '14:05:49'
    sprintf(time_output, " %s", update_time);
  }
  else
  {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo); // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);        // Creates: '02:05:49pm'
    sprintf(time_output, "%s %s", TXT_UPDATED, update_time);
  }
  Date_str = day_output;
  Time_str = time_output;
  return true;
}
//#########################################################################################
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.46;
  if (voltage > 1 ) { // Only display if there is a valid reading
    Serial.println("Voltage = " + String(voltage));
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.20) percentage = 0;  // orig 3.5
    display.drawRect(x + 25, y - 20, 40, 15, GxEPD_BLACK);
    display.fillRect(x + 65, y - 16, 4, 6, GxEPD_BLACK);
    display.fillRect(x + 27, y - 18, 36 * percentage / 100.0, 11, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(x + 5, y - 15, String(percentage) + "%", RIGHT);
    drawString(x + 13, y + 5,  String(voltage, 2) + "v", CENTER);
  }
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);    // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK); // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  //Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);            // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);            // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE);  // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE); // Right middle upper circle
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE); // Upper and lower lines
}
//#########################################################################################
void addraindrop(int x, int y, int scale) {
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
  x = x + scale * 1.6; y = y + scale / 3;
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale, uint8_t IconSize) {
  if (IconSize == SmallIcon) scale *= 1.34;
  for (int d = 0; d < 4; d++) {
    addraindrop(x + scale * (7.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale, uint8_t IconSize) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.1;
      display.drawLine(dxo + x + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 5; i++) {
    display.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, GxEPD_BLACK);
      if (scale == Large) display.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, GxEPD_BLACK);
      if (scale == Large) display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, GxEPD_BLACK);
      if (scale == Large) display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addsun(int x, int y, int scale, uint8_t IconSize) {
  int linesize = 3;
  if (IconSize == SmallIcon) linesize = 1;
  else if (IconSize == MediumIcon) linesize = 2;
  display.fillRect(x - scale * 2, y, scale * 4, linesize, GxEPD_BLACK);
  display.fillRect(x, y - scale * 2, linesize, scale * 4, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  if (IconSize != SmallIcon) {
    for (int o = 1; o <= linesize; o++) {
      display.drawLine(o + x - scale * 1.3, y - scale * 1.3, o + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
      display.drawLine(o + x - scale * 1.3, y + scale * 1.3, o + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    }
  }
  display.fillCircle(x, y, scale * 1.3, GxEPD_WHITE);
  display.fillCircle(x, y, scale, GxEPD_BLACK);
  display.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
}
//#########################################################################################
void addfog(int x, int y, int scale, int linesize, uint8_t IconSize) {
  if (IconSize == SmallIcon) {
    y -= 10;
    linesize = 1;
  }
  for (int i = 0; i < 6; i++) {
    display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize, GxEPD_BLACK);
  }
}
//#########################################################################################
void Sunny(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  if (IconSize == SmallIcon) y = y - 3; // Shift up small sun icon
  if (IconName.endsWith("n")) addmoon(x, y + 3, scale, IconSize);
  scale = scale * 1.6;
  addsun(x, y, scale, IconSize);
}
//#########################################################################################
void MostlySunny(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  int offset = (IconSize == LargeIcon) ? 10 : (IconSize == MediumIcon ? 6 : 5);
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
  addcloud(x, y + offset, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
}
//#########################################################################################
void MostlyCloudy(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void Cloudy(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  if (IconSize == SmallIcon) {
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    addcloud(x, y, scale, linesize);
  }
  else {
    y += (IconSize == MediumIcon ? 6 : 10);
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    addcloud(x + int(scale * 1.1), y - int(scale * 1.6), max(3, int(scale * 0.18)), linesize); // Cloud top right
    addcloud(x - int(scale * 0.7), y - int(scale * 1.1), max(4, int(scale * 0.25)), linesize); // Cloud top left
    addcloud(x, y, scale, linesize); // Main cloud
  }
}
//#########################################################################################
void Rain(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ExpectRain(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ChanceRain(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void Tstorms(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addtstorm(x, y, scale);
}
//#########################################################################################
void Snow(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsnow(x, y, scale, IconSize);
}
//#########################################################################################
void Fog(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y - 5, scale, linesize);
  addfog(x, y - 5, scale, linesize, IconSize);
}
//#########################################################################################
void Haze(int x, int y, uint8_t IconSize, String IconName) {
  int linesize = 3;
  int scale = iconScale(IconSize, linesize);
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x, y - 5, scale * 1.4, IconSize);
  addfog(x, y - 5, scale * 1.4, linesize, IconSize);
}
//#########################################################################################
void CloudCover(int x, int y, int CCover) {
  addcloud(x - 9, y - 3, Small * 0.8, 2); // Cloud top left
  addcloud(x + 3, y - 3, Small * 0.8, 2); // Cloud top right
  addcloud(x, y,         Small * 0.8, 2); // Main cloud
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(x + 34, y - 5, String(CCover) + "%", LEFT);
}
//#########################################################################################
void Visibility(int x, int y, String Visi) {
  y = y - 3; //
  float start_angle = 0.52, end_angle = 2.61;
  int r = 14;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    display.drawPixel(x + r * cos(i), y - r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i), GxEPD_BLACK);
  }
  start_angle = 3.61; end_angle = 5.78;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    display.drawPixel(x + r * cos(i), y + r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y + r / 2 + r * sin(i), GxEPD_BLACK);
  }
  display.fillCircle(x, y, r / 4, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(x + 18, y - 3, Visi, LEFT);
}
//#########################################################################################
void addmoon(int x, int y, int scale, uint8_t IconSize) {
  if (IconSize == LargeIcon) {
    display.fillCircle(x - 85, y - 100, uint16_t(scale * 0.8), GxEPD_BLACK);
    display.fillCircle(x - 57, y - 100, uint16_t(scale * 1.6), GxEPD_WHITE);
  }
  else if (IconSize == MediumIcon) {
    display.fillCircle(x - 42, y - 45, uint16_t(scale * 0.7), GxEPD_BLACK);
    display.fillCircle(x - 28, y - 45, uint16_t(scale * 1.2), GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 28, y - 37, uint16_t(scale * 1.0), GxEPD_BLACK);
    display.fillCircle(x - 20, y - 37, uint16_t(scale * 1.6), GxEPD_WHITE);
  }
}
//#########################################################################################
void Nodata(int x, int y, uint8_t IconSize, String IconName) {
  if (IconSize != SmallIcon) u8g2Fonts.setFont(u8g2_font_helvB24_tf); else u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 10, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}
//#########################################################################################
/* (C) D L BIRD
    This function will draw a graph on a ePaper/TFT/LCD display using data from an array containing data to be graphed.
    The variable 'max_readings' determines the maximum number of data elements for each array. Call it with the following parametric data:
    x_pos-the x axis top-left position of the graph
    y_pos-the y-axis top-left position of the graph, e.g. 100, 200 would draw the graph 100 pixels along and 200 pixels down from the top-left of the screen
    width-the width of the graph in pixels
    height-height of the graph in pixels
    Y1_Max-sets the scale of plotted data, for example 5000 would scale all data to a Y-axis of 5000 maximum
    data_array1 is parsed by value, externally they can be called anything else, e.g. within the routine it is called data_array1, but externally could be temperature_readings
    auto_scale-a logical value (TRUE or FALSE) that switches the Y-axis autoscale On or Off
    barchart_on-a logical value (TRUE or FALSE) that switches the drawing mode between barhcart and line graph
    barchart_colour-a sets the title and graph plotting colour
    If called with Y!_Max value of 500 and the data never goes above 500, then autoscale will retain a 0-500 Y scale, if on, the scale increases/decreases to match the data.
    auto_scale_margin, e.g. if set to 1000 then autoscale increments the scale by 1000 steps.
*/
void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int readings, boolean auto_scale, boolean barchart_mode) {
#define auto_scale_margin 0 // Sets the autoscale increment, so axis steps up in units of e.g. 3
#define y_minor_axis 5      // 5 y-axis division markers
  float maxYscale = -10000;
  float minYscale =  10000;
  int last_x, last_y;
  float x2, y2;
  if (auto_scale == true) {
    for (int i = 1; i < readings; i++ ) {
      if (DataArray[i] >= maxYscale) maxYscale = DataArray[i];
      if (DataArray[i] <= minYscale) minYscale = DataArray[i];
    }
    maxYscale = round(maxYscale + auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Max
    Y1Max = round(maxYscale + 0.5);
    if (minYscale != 0) minYscale = round(minYscale - auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Min
    Y1Min = round(minYscale);
  }
  // Draw the graph
  last_x = x_pos;
  last_y = y_pos + (Y1Max - constrain(DataArray[1], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;
  display.drawRect(x_pos, y_pos, gwidth + 3, gheight + 2, GxEPD_BLACK);
  drawString(x_pos + gwidth / 2, y_pos - 13, title, CENTER);
  // Draw the data
  for (int gx = 0; gx < readings; gx++) {
    y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
    if (barchart_mode) {
      x2 = x_pos + gx * (gwidth / readings) + 2;
      display.fillRect(x2, y2, (gwidth / readings) - 2, y_pos + gheight - y2 + 2, GxEPD_BLACK);
    } 
    else
    {
      x2 = x_pos + gx * gwidth / (readings - 1) + 1; // max_readings is the global variable that sets the maximum data that can be plotted
      display.drawLine(last_x, last_y, x2, y2, GxEPD_BLACK);
    }
    last_x = x2;
    last_y = y2;
  }
  //Draw the Y-axis scale
#define number_of_dashes 20
  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
    for (int j = 0; j < number_of_dashes; j++) { // Draw dashed graph grid lines
      if (spacing < y_minor_axis) display.drawFastHLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes), GxEPD_BLACK);
    }
    if ((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing) < 5 || title == TXT_PRESSURE_IN) {
        drawString(x_pos - 10, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
    }
    else
    {
      if (Y1Min < 1 && Y1Max < 10) {
        drawString(x_pos - 10, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
      }
      else {
        drawString(x_pos - 18, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0), RIGHT);
      }   
    }
  }
  for (int i = 0; i <= 2; i++) {
    drawString(5 + x_pos + gwidth / 3 * i, y_pos + gheight + 10, String(i), LEFT);
  }
  drawString(x_pos + gwidth / 2, y_pos + gheight + 25, TXT_DAYS, CENTER);
}

//#########################################################################################
void drawString(int x, int y, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}
//#########################################################################################
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y);
  if (text.length() > text_width * 2) {
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    text_width = 42;
    y = y - 3;
  }
  u8g2Fonts.println(text.substring(0, text_width));
  if (text.length() > text_width) {
    u8g2Fonts.setCursor(x, y + h + 15);
    String secondLine = text.substring(text_width);
    secondLine.trim(); // Remove any leading spaces
    u8g2Fonts.println(secondLine);
  }
}
//#########################################################################################
void InitialiseDisplay() {
  display.init(115200, true, 2, false);
  // GxEPD2_1248 sets up SPI itself with the pins given in the constructor, don't call SPI.begin() here
  u8g2Fonts.begin(display); // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}
//#########################################################################################
void ReportEvent(String EventMessage[]) {
  int y = int(SCREEN_HEIGHT - 40 * (EventCnt + 1) - 2 * 40);
  int wx = int (SCREEN_WIDTH * 0.1); 
  int wy = y;
  int tbw = int (SCREEN_WIDTH * 0.8);
  int tbh = int ((EventCnt + 1) * 40);
  if (EventCnt > EventThreshold) {
    display.setPartialWindow(wx, wy, tbw, tbh);
    display.fillRect(wx, wy, tbw, tbh, GxEPD_WHITE);
    display.drawRect(wx, wy, tbw, tbh, GxEPD_BLACK);
  }
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  for (byte Event = 1; Event <= EventCnt; Event++) {
    if (EventCnt > EventThreshold) drawString(wx + 3, wy + 20 + (Event - 1) * 30, 
                                   "Evt#" + String(Event < 10 ? "0" : "") + String(Event) + " : " + EventMessage[Event], LEFT);
    Serial.println("Evnt#" + String(Event < 10 ? "0" : "") + String(Event) + " : " + EventMessage[Event]);
  }
}
//#########################################################################################
void AddToEventLog(String message) {
  EventCnt++;
  EventMessage[EventCnt] = message;
}
//#########################################################################################
void VerboseRecordOfResetReason(RESET_REASON reason) {
  switch ( reason)  {
    case 1  : AddToEventLog("Vbat power on reset"); break;
    case 3  : AddToEventLog("Software reset digital core"); break;
    case 4  : AddToEventLog("Legacy watch dog reset digital core"); break;
    case 5  : AddToEventLog("Deep Sleep reset digital core"); break;
    case 6  : AddToEventLog("Reset by SLC module, reset digital core"); break;
    case 7  : AddToEventLog("Timer Group0 Watch dog reset digital core"); break;
    case 8  : AddToEventLog("Timer Group1 Watch dog reset digital core"); break;
    case 9  : AddToEventLog("RTC Watch dog Reset digital core"); break;
    case 10 : AddToEventLog("Instrusion tested to reset CPU"); break;
    case 11 : AddToEventLog("Time Group reset CPU"); break;
    case 12 : AddToEventLog("Software reset CPU"); break;
    case 13 : AddToEventLog("RTC Watch dog Reset CPU"); break;
    case 14 : AddToEventLog("APP CPU reset by PRO CPU"); break;
    case 15 : AddToEventLog("Reset when Vdd voltage is not stable"); break;
    case 16 : AddToEventLog("RTC Watch dog reset digital core and rtc module"); break;
    default : AddToEventLog("Unknown reason");
  }
}
//#########################################################################################
/*
  Version 1.0
   1. First Version of the weather display on an 9.7 Waveshare display
      Based on version 7_5_T7 for the 7.5 inch display written by G6EJD
   2. Trying to get accurate 30Min intervals. (the 'diff' variable added)
   3. added mDNS to assign to 'speaking' hostname
   4. The report event stuff from former weather display versions added
      Due to sporadic WiFi connection error or RX receive errors the InitialiseDisplay() 
      is moved to the begin of the setup function. If it is not moved, the ESP32 crashes,
      because the event report can't be written to the not initialized display.
   Version 1.1
   1. Added probability of precipitationto display e,g, 17%
   
   Version 1.2
   1. Adjusted graph drawing function to improve negative number drawing Line 1007

   Version 1.3
   1. Adapted display initialization for the 12.48 inch Waveshare display
   2. Adjusted layout to fit the larger screen
   3. Improved event log handling for the bigger display

   Version 1.4
   1. Replaced large wind compass with compact top-row "Wind" card showing
      tornado icon + wind speed and compass icon + direction acronym/degrees

   Version 1.5
   1. Display build version and short commit SHA in the top row as
      "Version <version> (<sha>)"

   Version 1.6
   1. Compacted current conditions into a 2x-wide top-row card with integrated
      condition description, medium weather icon, humidity, visibility, and cloud cover
   2. Moved date/time and Wi-Fi/battery status card directly below the top row
      on the left side of the display

   Version 1.7
   1. Removed humidity and pressure charts from the 3-day forecast, moved
      temperature and precipitation charts to the right, and centered headline
   2. Added bottom-left "Heutige Termine" section with side-by-side "cal1"
      and "cal2" event list cards

   Version 1.8
   1. Added Google Calendar fetch/decode via Apps Script bridge (src/calendar.h)
      and wired ReceiveTodayCalendars() into setup()
*/
