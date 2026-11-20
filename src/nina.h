#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define MAX_NINA_WARNINGS 4

struct NinaWarning {
  String headline;
  String provider;
  String severity;
  int severityRank;
  String sent;
};

struct NinaData {
  NinaWarning warnings[MAX_NINA_WARNINGS];
  int totalCount;
  bool valid;
};

extern NinaData ninaData;
String UrlEncode(const String& value);

/** Clear all cached NINA warnings and reset validity/counts. */
inline void ResetNinaData() {
  ninaData.totalCount = 0;
  ninaData.valid = false;
  for (int i = 0; i < MAX_NINA_WARNINGS; i++) {
    ninaData.warnings[i].headline     = "";
    ninaData.warnings[i].provider     = "";
    ninaData.warnings[i].severity     = "";
    ninaData.warnings[i].severityRank = 0;
    ninaData.warnings[i].sent         = "";
  }
}

/** Return true when a non-empty NINA regional key (ARS) is configured. */
inline bool HasConfiguredNina() {
  if (ninaApiArs == nullptr) return false;
  String ars(ninaApiArs);
  ars.trim();
  return ars.length() > 0;
}

/** Map CAP severity strings to descending numeric priority (Extreme=4 .. Unknown=0). */
inline int NinaSeverityRank(const String& severity) {
  String s = severity;
  s.trim();
  s.toLowerCase();
  if (s == "extreme")  return 4;
  if (s == "severe")   return 3;
  if (s == "moderate") return 2;
  if (s == "minor")    return 1;
  return 0;
}

/** Pick localized alert headline with safe fallbacks across i18nTitle and payload.data.headline. */
inline String ExtractNinaHeadline(JsonObjectConst item) {
  JsonObjectConst i18n = item["i18nTitle"].as<JsonObjectConst>();
  const char* headline = item["payload"]["data"]["headline"] | "";
  const char* deTitle  = i18n.isNull() ? "" : (i18n["de"] | "");
  const char* enTitle  = i18n.isNull() ? "" : (i18n["en"] | "");

  String lang = Language;
  lang.trim();
  lang.toUpperCase();

  String chosen;
  if (lang == "EN") {
    if (strlen(enTitle) > 0)       chosen = enTitle;
    else if (strlen(headline) > 0) chosen = headline;
    else if (strlen(deTitle) > 0)  chosen = deTitle;
  } else {
    if (strlen(deTitle) > 0)       chosen = deTitle;
    else if (strlen(headline) > 0) chosen = headline;
    else if (strlen(enTitle) > 0)  chosen = enTitle;
  }

  chosen.trim();
  if (chosen.length() == 0) chosen = "Warnung";
  return chosen;
}

/** Return true if warning a outranks warning b (higher severity first, then newer sent timestamp). */
inline bool NinaWarningComesBefore(const NinaWarning& a, const NinaWarning& b) {
  if (a.severityRank != b.severityRank) {
    return a.severityRank > b.severityRank;
  }
  return a.sent > b.sent;
}

/** Insert a decoded warning into the sorted top-MAX_NINA_WARNINGS buffer and update totalCount. */
inline void InsertNinaWarning(const NinaWarning& cand) {
  int stored = (ninaData.totalCount < MAX_NINA_WARNINGS) ? ninaData.totalCount : MAX_NINA_WARNINGS;
  int pos = stored;
  for (int i = 0; i < stored; i++) {
    if (NinaWarningComesBefore(cand, ninaData.warnings[i])) {
      pos = i;
      break;
    }
  }
  if (pos < MAX_NINA_WARNINGS) {
    int last = (stored < MAX_NINA_WARNINGS) ? stored : (MAX_NINA_WARNINGS - 1);
    for (int j = last; j > pos; j--) {
      ninaData.warnings[j] = ninaData.warnings[j - 1];
    }
    ninaData.warnings[pos] = cand;
  }
  ninaData.totalCount++;
}

/** Decode a NINA /dashboard/{ARS}.json payload using a field filter and store top warnings by severity. */
inline bool DecodeNinaJson(const String& payload, bool print) {
  ResetNinaData();

  JsonDocument filter;
  filter[0]["sent"] = true;
  filter[0]["payload"]["data"]["headline"] = true;
  filter[0]["payload"]["data"]["provider"] = true;
  filter[0]["payload"]["data"]["severity"] = true;
  filter[0]["i18nTitle"]["de"] = true;
  filter[0]["i18nTitle"]["en"] = true;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error) {
    Serial.print("deserializeJson(nina) failed: ");
    Serial.println(error.c_str());
    return false;
  }

  if (!doc.is<JsonArrayConst>()) {
    Serial.println("NINA API error: expected JSON array");
    return false;
  }

  JsonArrayConst arr = doc.as<JsonArrayConst>();
  for (JsonObjectConst item : arr) {
    JsonObjectConst data = item["payload"]["data"].as<JsonObjectConst>();
    NinaWarning warn;
    warn.headline     = ExtractNinaHeadline(item);
    warn.provider     = data.isNull() ? String("") : String(data["provider"] | "");
    warn.provider.trim();
    if (warn.provider.length() == 0) warn.provider = "NINA";
    warn.severity     = data.isNull() ? String("") : String(data["severity"] | "");
    warn.severity.trim();
    warn.severityRank = NinaSeverityRank(warn.severity);
    warn.sent         = String(item["sent"] | "");
    warn.sent.trim();
    InsertNinaWarning(warn);
  }

  ninaData.valid = true;

  if (print) {
    Serial.printf("Decoded NINA warnings: %d active (displaying %d)\n",
                  ninaData.totalCount,
                  (ninaData.totalCount < MAX_NINA_WARNINGS) ? ninaData.totalCount : MAX_NINA_WARNINGS);
  }
  return true;
}

/** Fetch active civil protection & weather warnings from the NINA dashboard endpoint. */
inline bool ReceiveNinaWarnings(bool print) {
  ResetNinaData();
  if (!HasConfiguredNina()) return false;

  String ars(ninaApiArs);
  ars.trim();
  String url = "https://warnung.bund.de/api31/dashboard/" + UrlEncode(ars) + ".json";

  if (print) Serial.println("Rx NINA warnings...");
  WiFiClientSecure secureClient;
  // Skip CA pinning so root-CA rotation does not break unattended refreshes.
  secureClient.setInsecure();

  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(secureClient, url)) {
    Serial.println("NINA http.begin failed");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("NINA failed, http=%d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();
  return DecodeNinaJson(payload, print);
}
