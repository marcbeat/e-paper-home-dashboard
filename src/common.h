//

#include <Arduino.h>
#include <HTTPClient.h>

typedef struct { // For current Day and Day 1, 2, 3, etc
  String Time;
  float  High;
  float  Low;
} HL_record_type;

HL_record_type  HLReadings[max_readings];

typedef struct { // For current Day and Day 1, 2, 3, etc
  int      Dt;
  String   Period;
  String   Icon;
  String   Trend;
  String   Main0;
  String   Forecast0;
  String   Forecast1;
  String   Forecast2;
  String   Description;
  String   Time;
  String   Country;
  float    lat;
  float    lon;
  float    Temperature;
  float    FeelsLike;
  float    Humidity;
  float    DewPoint;
  float    High;
  float    Low;
  float    Winddir;
  float    Windspeed;
  float    Rainfall;
  float    Snowfall;
  float    Pressure;
  int      Cloudcover;
  int      Visibility;
  int      Sunrise;
  int      Sunset;
  int      Moonrise;
  int      Moonset;
  int      Timezone;
  float    UVI;
  float    PoP;
} Forecast_record_type;

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];
Forecast_record_type  Daily[8];

bool ReceiveOneCallWeather(WiFiClient& client, bool print);
bool DecodeWeather25(WiFiClient& json, bool print);
bool DecodeForecast25(WiFiClient& json, bool print);
void Convert_Readings_to_Imperial();
String ConvertUnixTime(int unix_time);
float mm_to_inches(float value_mm);
float hPa_to_inHg(float value_hPa);
int JulianDate(int d, int m, int y);
float SumOfPrecip(float DataArray[], int readings);
String TitleCase(String text);
double NormalizedMoonPhase(int d, int m, int y);

//#########################################################################################
void Convert_Readings_to_Imperial() {
  WxConditions[0].Pressure = hPa_to_inHg(WxConditions[0].Pressure);
  WxForecast[1].Rainfall   = mm_to_inches(WxForecast[1].Rainfall);
  WxForecast[1].Snowfall   = mm_to_inches(WxForecast[1].Snowfall);
}
//#########################################################################################
String ConvertUnixTime(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  time_t tm = unix_time;
  struct tm *now_tm = gmtime(&tm);
  char output[40];
  if (Units == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
  }
  else {
    strftime(output, sizeof(output), "%I:%M%P %m/%d/%y", now_tm);
  }
  return output;
}
//#########################################################################################
// OpenWeatherMap free API 2.5 (/data/2.5/weather + /data/2.5/forecast)
bool ReceiveOneCallWeather(WiFiClient& client, bool print) {
  const String units = (Units == "M" ? "metric" : "imperial");
  HTTPClient http;

  // 1. Current weather
  Serial.println("Rx current weather (OWM 2.5)...");
  client.stop();
  String uri = "/data/2.5/weather?lat=" + LAT + "&lon=" + LON + "&appid=" + apikey + "&mode=json&units=" + units + "&lang=" + Language;
  http.begin(client, server, 80, uri);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("weather failed, http=%d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
  bool ok = DecodeWeather25(http.getStream(), print);
  client.stop();
  http.end();
  if (!ok) return false;

  // 2. 3-hourly forecast (24 readings = 3 days)
  Serial.println("Rx forecast (OWM 2.5)...");
  String furi = "/data/2.5/forecast?lat=" + LAT + "&lon=" + LON + "&appid=" + apikey + "&mode=json&units=" + units + "&lang=" + Language + "&cnt=" + String(max_readings);
  http.begin(client, server, 80, furi);
  httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("forecast failed, http=%d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
  ok = DecodeForecast25(http.getStream(), print);
  client.stop();
  http.end();
  return ok;
}
//#######################################################################################
bool DecodeWeather25(WiFiClient& json, bool print) {
  if (print) Serial.println("Decoding current weather...");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print("deserializeJson(weather) failed: ");
    Serial.println(error.c_str());
    return false;
  }
  WxConditions[0].Timezone    = doc["timezone"] | 0;
  WxConditions[0].Main0       = doc["weather"][0]["main"] | "";
  WxConditions[0].Forecast0   = doc["weather"][0]["description"] | "";
  WxConditions[0].Forecast1   = doc["weather"][1]["description"] | "";
  WxConditions[0].Forecast2   = doc["weather"][2]["description"] | "";
  WxConditions[0].Description = WxConditions[0].Forecast0;
  WxConditions[0].Icon        = doc["weather"][0]["icon"] | "";
  WxConditions[0].Sunrise     = doc["sys"]["sunrise"] | 0;
  WxConditions[0].Sunset      = doc["sys"]["sunset"] | 0;
  WxConditions[0].Temperature = doc["main"]["temp"] | 0.0f;
  WxConditions[0].FeelsLike   = doc["main"]["feels_like"] | 0.0f;
  WxConditions[0].High        = doc["main"]["temp_max"] | 0.0f;
  WxConditions[0].Low         = doc["main"]["temp_min"] | 0.0f;
  WxConditions[0].Pressure    = doc["main"]["pressure"] | 0.0f;
  WxConditions[0].Humidity    = doc["main"]["humidity"] | 0.0f;
  WxConditions[0].Cloudcover  = doc["clouds"]["all"] | 0;
  WxConditions[0].Visibility  = doc["visibility"] | 0;
  WxConditions[0].Windspeed   = doc["wind"]["speed"] | 0.0f;
  WxConditions[0].Winddir     = doc["wind"]["deg"] | 0.0f;
  if (print) {
    Serial.println("Fore: " + WxConditions[0].Forecast0 + ", Icon: " + WxConditions[0].Icon);
    Serial.println("Temp: " + String(WxConditions[0].Temperature) + " (" + String(WxConditions[0].Low) + "/" + String(WxConditions[0].High) + ")");
    Serial.println("Pres: " + String(WxConditions[0].Pressure) + ", Humi: " + String(WxConditions[0].Humidity));
  }
  return true;
}
//#######################################################################################
bool DecodeForecast25(WiFiClient& json, bool print) {
  if (print) Serial.println("Decoding forecast...");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print("deserializeJson(forecast) failed: ");
    Serial.println(error.c_str());
    return false;
  }
  JsonArray list = doc["list"].as<JsonArray>();
  int count = list.size();
  if (count > max_readings) count = max_readings;
  for (int r = 0; r < count; r++) {
    JsonObject item = list[r];
    WxForecast[r].Dt          = item["dt"] | 0;
    WxForecast[r].Temperature = item["main"]["temp"] | 0.0f;
    WxForecast[r].FeelsLike   = item["main"]["feels_like"] | 0.0f;
    WxForecast[r].High        = item["main"]["temp_max"] | 0.0f;
    WxForecast[r].Low         = item["main"]["temp_min"] | 0.0f;
    WxForecast[r].Pressure    = item["main"]["pressure"] | 0.0f;
    WxForecast[r].Humidity    = item["main"]["humidity"] | 0.0f;
    WxForecast[r].PoP         = item["pop"] | 0.0f;
    WxForecast[r].Rainfall    = item["rain"]["3h"] | 0.0f;
    WxForecast[r].Snowfall    = item["snow"]["3h"] | 0.0f;
    WxForecast[r].Icon        = item["weather"][0]["icon"] | "";
  }
  if (print) Serial.println("Decoded " + String(count) + " forecast entries.");
  float pressure_trend = WxForecast[0].Pressure - WxForecast[2].Pressure;
  pressure_trend = ((int)(pressure_trend * 10)) / 10.0;
  WxConditions[0].Trend = "=";
  if (pressure_trend > 0)  WxConditions[0].Trend = "+";
  if (pressure_trend < 0)  WxConditions[0].Trend = "-";
  if (pressure_trend == 0) WxConditions[0].Trend = "0";
  if (Units == "I") Convert_Readings_to_Imperial();
  return true;
}

float mm_to_inches(float value_mm){
  return 0.0393701 * value_mm;
}

float hPa_to_inHg(float value_hPa){
  return 0.02953 * value_hPa;
}

int JulianDate(int d, int m, int y) {
  int mm, yy, k1, k2, k3, j;
  yy = y - (int)((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12) mm = mm - 12;
  k1 = (int)(365.25 * (yy + 4712));
  k2 = (int)(30.6001 * mm + 0.5);
  k3 = (int)((int)((yy / 100) + 49) * 0.75) - 38;
  // 'j' for dates in Julian calendar:
  j = k1 + k2 + d + 59 + 1;
  if (j > 2299160) j = j - k3; // 'j' is the Julian date at 12h UT (Universal Time) For Gregorian calendar:
  return j;
}

float SumOfPrecip(float DataArray[], int readings) {
  float sum = 0;
  for (int i = 0; i < readings; i++) {
    sum += DataArray[i];
  }
  return sum;
}

String TitleCase(String text){
  if (text.length() > 0) {
    String temp_text = text.substring(0,1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1); // Title-case the string
  }
  else return text;
}

double NormalizedMoonPhase(int d, int m, int y) {
  int j = JulianDate(d, m, y);
  //Calculate the approximate phase of the moon
  double Phase = (j + 4.867) / 29.53059;
  return (Phase - (int) Phase);
}


