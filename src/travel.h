#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>

#define MAX_TRAVEL_DESTS 2

struct TravelRouteData {
  String name;
  bool configured;
  bool valid;
  int durationMin;
  int delayMin;
};

extern TravelRouteData travelRoutes[MAX_TRAVEL_DESTS];

/** Reset all travel route slots to their configured defaults and -1 sentinel values. */
inline void ResetTravelRoutes() {
  for (int i = 0; i < MAX_TRAVEL_DESTS; i++) {
    bool hasName = (travelDestNames[i] != nullptr && strlen(travelDestNames[i]) > 0);
    bool hasLat  = (travelDestLats[i]  != nullptr && strlen(travelDestLats[i])  > 0);
    bool hasLon  = (travelDestLons[i]  != nullptr && strlen(travelDestLons[i])  > 0);
    travelRoutes[i].name        = hasName ? String(travelDestNames[i]) : ("Ziel " + String(i + 1));
    travelRoutes[i].configured  = (hasLat && hasLon);
    travelRoutes[i].valid       = false;
    travelRoutes[i].durationMin = -1;
    travelRoutes[i].delayMin    = -1;
  }
}

/** Return true when a Mapbox token, origin LAT/LON, and at least one destination are configured. */
inline bool HasConfiguredTravelDests() {
  if (mapboxAccessToken == nullptr || strlen(mapboxAccessToken) == 0) return false;
  if (LAT.length() == 0 || LON.length() == 0) return false;
  for (int i = 0; i < MAX_TRAVEL_DESTS; i++) {
    if (travelDestLats[i] != nullptr && strlen(travelDestLats[i]) > 0 &&
        travelDestLons[i] != nullptr && strlen(travelDestLons[i]) > 0) {
      return true;
    }
  }
  return false;
}

/** Parse a Mapbox Directions API JSON payload into travelRoutes[index]. */
inline bool DecodeTravelJson(const String& payload, int index, bool print) {
  if (index < 0 || index >= MAX_TRAVEL_DESTS) return false;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("deserializeJson(travel) failed: ");
    Serial.println(error.c_str());
    return false;
  }

  if (String(doc["code"] | "") != "Ok") {
    Serial.println("Travel API error: " + String(doc["code"] | "unknown"));
    return false;
  }

  JsonObjectConst route = doc["routes"][0].as<JsonObjectConst>();
  if (route.isNull()) {
    Serial.println("Travel API error: missing route");
    return false;
  }

  double durSec = route["duration"] | -1.0;
  if (durSec < 0) {
    Serial.println("Travel API error: missing duration");
    return false;
  }

  travelRoutes[index].durationMin = (int)round(durSec / 60.0);

  JsonVariantConst typVar = route["duration_typical"];
  if (typVar.is<float>() || typVar.is<double>() || typVar.is<long>()) {
    int delta = (int)round((durSec - typVar.as<double>()) / 60.0);
    travelRoutes[index].delayMin = (delta > 0) ? delta : 0;
  } else {
    travelRoutes[index].delayMin = -1;
  }

  travelRoutes[index].valid = true;

  if (print) {
    Serial.printf("Decoded travel[%d] (%s): %d min (delay=%d)\n",
                  index, travelRoutes[index].name.c_str(),
                  travelRoutes[index].durationMin, travelRoutes[index].delayMin);
  }
  return true;
}

/** Fetch live driving-traffic travel times for all configured destinations from Mapbox Directions. */
inline bool ReceiveTravelTimes(bool print) {
  ResetTravelRoutes();
  if (!HasConfiguredTravelDests()) return false;

  WiFiClientSecure secureClient;
  // Skip CA pinning so root-CA rotation does not break unattended refreshes.
  secureClient.setInsecure();

  HTTPClient http;
  http.setTimeout(10000);

  bool allOk = true;
  for (int i = 0; i < MAX_TRAVEL_DESTS; i++) {
    if (!travelRoutes[i].configured) continue;

    String url = "https://api.mapbox.com/directions/v5/mapbox/driving-traffic/"
                 + UrlEncode(LON) + "," + UrlEncode(LAT)
                 + ";" + UrlEncode(travelDestLons[i]) + "," + UrlEncode(travelDestLats[i])
                 + "?overview=false&steps=false&depart_at=now&access_token="
                 + UrlEncode(mapboxAccessToken);

    bool destOk = false;
    for (byte attempt = 1; !destOk && attempt <= 2; attempt++) {
      if (print) {
        Serial.printf("Rx travel[%d] (%s) attempt %d...\n",
                      i, travelRoutes[i].name.c_str(), attempt);
      }

      if (!http.begin(secureClient, url)) {
        Serial.printf("Travel[%d] http.begin failed\n", i);
        continue;
      }

      int httpCode = http.GET();
      if (httpCode != HTTP_CODE_OK) {
        Serial.printf("travel[%d] failed, http=%d (%s)\n",
                      i, httpCode, http.errorToString(httpCode).c_str());
        http.end();
        continue;
      }

      String payload = http.getString();
      http.end();
      destOk = DecodeTravelJson(payload, i, print);
    }

    if (!destOk) {
      travelRoutes[i].valid = false;
      allOk = false;
    }
  }

  return allOk;
}
