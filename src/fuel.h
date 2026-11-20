#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define MAX_FUEL_STATIONS 3

struct FuelStationData {
  String name;
  bool configured;
  bool valid;
  bool isOpen;
  float e5;
  float e10;
  float diesel;
};

extern FuelStationData fuelStations[MAX_FUEL_STATIONS];

/** Reset all fuel station slots to their configured defaults and -1.0f sentinel prices. */
inline void ResetFuelStations() {
  for (int i = 0; i < MAX_FUEL_STATIONS; i++) {
    bool hasName = (fuelStationNames[i] != nullptr && strlen(fuelStationNames[i]) > 0);
    bool hasId   = (fuelStationIds[i]   != nullptr && strlen(fuelStationIds[i]) > 0);
    fuelStations[i].name       = hasName ? String(fuelStationNames[i]) : ("Station " + String(i + 1));
    fuelStations[i].configured = hasId;
    fuelStations[i].valid      = false;
    fuelStations[i].isOpen     = false;
    fuelStations[i].e5         = -1.0f;
    fuelStations[i].e10        = -1.0f;
    fuelStations[i].diesel     = -1.0f;
  }
}

/** Return true when a Tankerkoenig API key and at least one station ID are configured. */
inline bool HasConfiguredFuelStations() {
  if (tankerkoenigApiKey == nullptr || strlen(tankerkoenigApiKey) == 0) return false;
  for (int i = 0; i < MAX_FUEL_STATIONS; i++) {
    if (fuelStationIds[i] != nullptr && strlen(fuelStationIds[i]) > 0) {
      return true;
    }
  }
  return false;
}

/** Extract a numeric fuel price; return -1.0f for boolean false (closed/unavailable) or null. */
inline float ExtractFuelPrice(JsonVariantConst v) {
  if (v.is<float>() || v.is<double>()) {
    return v.as<float>();
  }
  return -1.0f;
}

/** Parse a Tankerkoenig prices.php JSON payload into fuelStations[]. */
inline bool DecodeFuelJson(const String& payload, bool print) {
  ResetFuelStations();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("deserializeJson(fuel) failed: ");
    Serial.println(error.c_str());
    return false;
  }

  if (!(doc["ok"] | false)) {
    Serial.println("Fuel API error: " + String(doc["message"] | "ok=false"));
    return false;
  }

  JsonObjectConst prices = doc["prices"].as<JsonObjectConst>();
  if (prices.isNull()) {
    Serial.println("Fuel API error: missing prices object");
    return false;
  }

  int validCount = 0;
  for (int i = 0; i < MAX_FUEL_STATIONS; i++) {
    if (!fuelStations[i].configured) continue;
    JsonObjectConst entry = prices[fuelStationIds[i]].as<JsonObjectConst>();
    if (!entry.isNull()) {
      fuelStations[i].valid  = true;
      fuelStations[i].isOpen = (String(entry["status"] | "") == "open");
      fuelStations[i].e5     = ExtractFuelPrice(entry["e5"]);
      fuelStations[i].e10    = ExtractFuelPrice(entry["e10"]);
      fuelStations[i].diesel = ExtractFuelPrice(entry["diesel"]);
      validCount++;
    }
  }

  if (print) {
    Serial.printf("Decoded fuel prices: %d station(s)\n", validCount);
  }
  return true;
}

/** Fetch live fuel prices for configured stations from Tankerkoenig prices.php. */
inline bool ReceiveFuelPrices(bool print) {
  ResetFuelStations();
  if (!HasConfiguredFuelStations()) return false;

  String ids;
  for (int i = 0; i < MAX_FUEL_STATIONS; i++) {
    if (fuelStationIds[i] != nullptr && strlen(fuelStationIds[i]) > 0) {
      if (ids.length() > 0) ids += ',';
      ids += fuelStationIds[i];
    }
  }

  String url = "https://creativecommons.tankerkoenig.de/json/prices.php?ids="
               + UrlEncode(ids)
               + "&apikey="
               + UrlEncode(tankerkoenigApiKey);

  if (print) Serial.println("Rx fuel prices...");
  WiFiClientSecure secureClient;
  // Skip CA pinning so Let's Encrypt / root-CA rotation does not break unattended refreshes.
  secureClient.setInsecure();

  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(secureClient, url)) {
    Serial.println("Fuel http.begin failed");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("fuel failed, http=%d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();
  return DecodeFuelJson(payload, print);
}
