#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define MAX_CALENDAR_EVENTS 10

struct CalendarEvent {
  String time;
  String title;
  bool allDay;
};

extern CalendarEvent pietEvents[MAX_CALENDAR_EVENTS];
extern CalendarEvent natiEvents[MAX_CALENDAR_EVENTS];
extern int pietEventCount;
extern int natiEventCount;

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

/** Parse the Apps Script bridge JSON payload into pietEvents and natiEvents. */
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
  PopulateEventList(doc["piet"].as<JsonArrayConst>(), pietEvents, pietEventCount);
  PopulateEventList(doc["nati"].as<JsonArrayConst>(), natiEvents, natiEventCount);
  if (print) {
    Serial.printf("Decoded calendar events: Piet=%d, Nati=%d\n", pietEventCount, natiEventCount);
  }
  return true;
}

/** Fetch today's events for Piet and Nati from the configured Google Apps Script bridge. */
bool ReceiveTodayCalendars(bool print) {
  if (strlen(calendarBridgeUrl) == 0) return false;

  String url = String(calendarBridgeUrl);
  url += (url.indexOf('?') >= 0) ? '&' : '?';
  url += "token=" + UrlEncode(calendarBridgeToken);
  url += "&piet=" + UrlEncode(calendarIdPiet);
  url += "&nati=" + UrlEncode(calendarIdNati);

  if (print) Serial.println("Rx today calendars...");
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
