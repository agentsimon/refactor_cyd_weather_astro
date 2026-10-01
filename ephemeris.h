#ifndef EPHEMERIS_H
#define EPHEMERIS_H

#include "globals.h"

/*
  ephemeris.h
  -----------
  Fetches a single planetary-positions snapshot for right now from
  FreeAstroAPI (https://www.freeastroapi.com/docs/western/ephemeris).

  Only "start" is sent (no "end"/"step"), so the API returns one
  snapshot rather than a date-range table - exactly the current
  positions, which is all the Ephemeris screen shows. "start" is the
  board's NTP-synced UTC time, colon-encoded as %%3A since HTTPClient
  doesn't URL-encode query strings for us.

  Reads numeric degree_in_sign/sign_abbr/retrograde fields rather than
  the ready-made "position_text" string, since position_text contains
  a Unicode degree symbol (°) that the TFT's built-in fonts can't
  render.
*/

bool fetchEphemeris() {
  Serial.println("=== fetchEphemeris() ===");
  ephemerisCount = 0;
  ephemerisLimitReached = false;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnect...");
    WiFi.reconnect();
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(300);
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Still not connected, aborting ephemeris fetch.");
      return false;
    }
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5000)) {
    Serial.println("NTP time not available yet - aborting ephemeris fetch.");
    return false;
  }

  // Reset the counter if the UTC calendar date has rolled over since the
  // last call (encode as YYYY*1000+day-of-year so it's a single int compare).
  int todayKey = (timeinfo.tm_year + 1900) * 1000 + timeinfo.tm_yday;
  if (todayKey != ephemerisCallDayKey) {
    ephemerisCallDayKey = todayKey;
    ephemerisCallsToday = 0;
  }

  if (ephemerisCallsToday >= EPHEMERIS_MAX_CALLS_PER_DAY) {
    Serial.printf("Ephemeris daily call limit reached (%d/%d) - skipping request.\n",
                  ephemerisCallsToday, EPHEMERIS_MAX_CALLS_PER_DAY);
    ephemerisLimitReached = true;
    return false;
  }

  char urlTime[32];    // colon-encoded, for the request
  char displayTime[24]; // plain, for on-screen display
  snprintf(urlTime, sizeof(urlTime), "%04d-%02d-%02dT%02d%%3A%02d%%3A%02dZ",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  snprintf(displayTime, sizeof(displayTime), "%04d-%02d-%02d %02d:%02d:%02d",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  ephemerisTimeLabel = String(displayTime);

  String bodiesParam = "";
  for (int i = 0; i < NUM_EPHEMERIS_BODIES; i++) {
    if (i > 0) bodiesParam += ",";
    bodiesParam += EPHEMERIS_BODIES[i];
  }

  String url = "https://api.freeastroapi.com/api/v1/ephemeris?start=" + String(urlTime) +
               "&bodies=" + bodiesParam;

  // Count the call as soon as we're committed to sending it, so a failed
  // request still counts against the cap - it still cost a call at the API.
  ephemerisCallsToday++;
  Serial.printf("Ephemeris calls today: %d/%d\n", ephemerisCallsToday, EPHEMERIS_MAX_CALLS_PER_DAY);

  Serial.print("Requesting URL: ");
  Serial.println(url);

  HTTPClient http;
  http.begin(secureClient, url);
  http.addHeader("x-api-key", EPHEMERIS_API_KEY);
  int httpCode = http.GET();
  Serial.printf("HTTP response code: %d\n", httpCode);

  if (httpCode != 200) {
    Serial.println("Non-200 response, dumping body for debugging:");
    Serial.println(http.getString());
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  Serial.printf("Payload length: %d bytes\n", payload.length());

  JSONVar doc = JSON.parse(payload);
  if (JSON.typeof(doc) == "undefined") {
    Serial.println("Ephemeris JSON parse FAILED");
    return false;
  }

  JSONVar bodiesObj = doc["data"]["bodies"];
  if (JSON.typeof(bodiesObj) == "undefined") {
    Serial.println("Ephemeris JSON missing 'data.bodies' field.");
    return false;
  }

  for (int i = 0; i < NUM_EPHEMERIS_BODIES; i++) {
    JSONVar body = bodiesObj[EPHEMERIS_BODIES[i]];
    if (JSON.typeof(body) == "undefined") {
      Serial.printf("  Ephemeris response missing body: %s\n", EPHEMERIS_BODIES[i]);
      continue;
    }

    ephemerisName[ephemerisCount]   = String(EPHEMERIS_BODIES[i]);
    ephemerisSign[ephemerisCount]   = (const char*)body["sign_abbr"];
    ephemerisDegree[ephemerisCount] = (float)(double)body["degree_in_sign"];
    ephemerisRetro[ephemerisCount]  = (bool)body["retrograde"];

    Serial.printf("  %-8s %.1f %s%s\n", EPHEMERIS_BODIES[i], ephemerisDegree[ephemerisCount],
                  ephemerisSign[ephemerisCount].c_str(), ephemerisRetro[ephemerisCount] ? " Rx" : "");

    ephemerisCount++;
  }

  Serial.printf("Ephemeris bodies parsed: %d\n", ephemerisCount);
  return ephemerisCount > 0;
}

#endif // EPHEMERIS_H
