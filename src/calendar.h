#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#define MAX_CALENDAR_EVENTS 10

struct CalendarEvent {
  String time;
  String title;
  bool allDay;
};

struct CalendarDayEvents {
  CalendarEvent events[MAX_CALENDAR_EVENTS];
  int count = 0;
};

struct CalendarData {
  CalendarDayEvents today;
  CalendarDayEvents tomorrow;
};

extern CalendarData cal1Data;
extern CalendarData cal2Data;

/** Percent-encode a query parameter value for HTTP GET requests. */
String UrlEncode(const String& value) {
  static const char hex[] = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(value.length());
  for (size_t i = 0; i < value.length(); i++) {
    uint8_t c = static_cast<uint8_t>(value[i]);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

/** Copy up to MAX_CALENDAR_EVENTS entries from a JSON array into an event list. */
void PopulateEventList(JsonArrayConst arr, CalendarEvent out[], int& outCount) {
  outCount = 0;
  if (arr.isNull()) return;
  for (JsonObjectConst item : arr) {
    if (outCount >= MAX_CALENDAR_EVENTS) break;
    out[outCount].time   = item["time"]   | "";
    out[outCount].title  = item["title"]  | "";
    out[outCount].allDay = item["allDay"] | false;
    outCount++;
  }
}

/** Format the current local NTP time as ISO-8601 (YYYY-MM-DDTHH:MM:SS+HH:MM). */
String FormatLocalIsoDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000)) return "";
  char dtBuf[24];
  char tzBuf[8];
  strftime(dtBuf, sizeof(dtBuf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  strftime(tzBuf, sizeof(tzBuf), "%z", &timeinfo);
  String result(dtBuf);
  if (strlen(tzBuf) == 5 && (tzBuf[0] == '+' || tzBuf[0] == '-')) {
    result += String(tzBuf).substring(0, 3) + ":" + String(tzBuf).substring(3);
  }
  return result;
}

/** Return true if a calendar JSON node is an object containing today and tomorrow arrays. */
bool HasValidCalendarSchema(JsonVariantConst val) {
  return val.is<JsonObjectConst>() &&
         val["today"].is<JsonArrayConst>() &&
         val["tomorrow"].is<JsonArrayConst>();
}

/** Populate today and tomorrow event lists for a single calendar JSON object. */
void PopulateCalendarData(JsonObjectConst obj, CalendarData& data) {
  PopulateEventList(obj["today"].as<JsonArrayConst>(), data.today.events, data.today.count);
  PopulateEventList(obj["tomorrow"].as<JsonArrayConst>(), data.tomorrow.events, data.tomorrow.count);
}

/** Parse the Apps Script bridge JSON payload into cal1Data and cal2Data. */
bool DecodeCalendarJson(const String& payload, bool print) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("deserializeJson(calendar) failed: ");
    Serial.println(error.c_str());
    return false;
  }
  if (!doc["error"].isNull()) {
    Serial.println("Calendar bridge error: " + String(doc["error"].as<const char*>()));
    return false;
  }
  if (!HasValidCalendarSchema(doc["cal1"]) || !HasValidCalendarSchema(doc["cal2"])) {
    Serial.println("Calendar JSON schema mismatch: expected cal1/cal2 objects with today/tomorrow arrays");
    return false;
  }
  PopulateCalendarData(doc["cal1"].as<JsonObjectConst>(), cal1Data);
  PopulateCalendarData(doc["cal2"].as<JsonObjectConst>(), cal2Data);
  if (print) {
    Serial.printf("Decoded calendar events: cal1(today=%d, tomorrow=%d), cal2(today=%d, tomorrow=%d)\n",
                  cal1Data.today.count, cal1Data.tomorrow.count,
                  cal2Data.today.count, cal2Data.tomorrow.count);
  }
  return true;
}

/** Fetch today's and tomorrow's events for cal1 and cal2 from the configured Google Apps Script bridge. */
bool ReceiveCalendars(bool print) {
  if (strlen(calendarBridgeUrl) == 0) return false;

  String url = String(calendarBridgeUrl);
  url += (url.indexOf('?') >= 0) ? '&' : '?';
  url += "token=" + UrlEncode(calendarBridgeToken);
  url += "&cal1=" + UrlEncode(calendarId1);
  url += "&cal2=" + UrlEncode(calendarId2);
  String dt = FormatLocalIsoDateTime();
  if (dt.length() > 0) {
    url += "&datetime=" + UrlEncode(dt);
  }

  if (print) Serial.println("Rx calendars...");
  WiFiClientSecure secureClient;
  // Google Apps Script redirects across google.com / googleusercontent.com;
  // skip CA pinning so root-CA rotation does not break unattended refreshes.
  secureClient.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  if (!http.begin(secureClient, url)) {
    Serial.println("Calendar http.begin failed");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("calendar failed, http=%d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();
  return DecodeCalendarJson(payload, print);
}
