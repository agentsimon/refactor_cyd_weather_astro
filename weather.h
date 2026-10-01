#ifndef WEATHER_H
#define WEATHER_H

#include "globals.h"

/*
  weather.h
  ---------
  Fetches current conditions for the configured METAR station from
  aviationweather.gov's free, keyless Data API, and derives the values
  the display needs (humidity, rain condition, weather icon) from it.

  METAR doesn't give humidity directly - it's calculated here from
  temperature and dew point. It doesn't give a clean precipitation
  amount either, so rain is reported as a human-readable condition
  ("No Rain" / "Light Rain" / "Moderate Rain" / "Heavy Rain") parsed
  from the raw METAR text instead of a millimeter figure.
*/

// -----------------------------------------------------------------
// Computes relative humidity (%) from temperature and dew point (both
// in Celsius) using the Magnus-Tetens approximation. METAR doesn't
// report RH directly, only temp and dewpoint, so this is the standard
// way to derive it - it's the same formula aviation weather software
// commonly uses internally.
// -----------------------------------------------------------------
float computeRelativeHumidity(float tempC, float dewpC) {
  float eDew  = 6.1094 * exp((17.625 * dewpC) / (243.04 + dewpC));
  float eTemp = 6.1094 * exp((17.625 * tempC) / (243.04 + tempC));
  float rh = 100.0 * (eDew / eTemp);
  if (rh < 0) rh = 0;
  if (rh > 100) rh = 100;
  return rh;
}

// -----------------------------------------------------------------
// Derives a WeatherIconType from a METAR's present-weather string
// (wxString, e.g. "-RA", "+TSRA", "BR") and, if there's no significant
// weather reported, from its cloud cover layers instead.
// -----------------------------------------------------------------
WeatherIconType metarToIcon(JSONVar &metar) {
  JSONVar wxVar = metar["wxString"];
  String wx = (JSON.typeof(wxVar) == "string") ? (const char*)wxVar : "";
  wx.toUpperCase();

  if (wx.indexOf("TS") >= 0) return ICON_THUNDERSTORM;
  if (wx.indexOf("SN") >= 0 || wx.indexOf("GS") >= 0 || wx.indexOf("IC") >= 0) return ICON_SNOW;
  if (wx.indexOf("RA") >= 0 || wx.indexOf("DZ") >= 0 || wx.indexOf("SH") >= 0) return ICON_RAIN;
  if (wx.indexOf("FG") >= 0 || wx.indexOf("BR") >= 0 || wx.indexOf("HZ") >= 0) return ICON_FOG;

  // No significant weather reported - fall back to sky cover.
  JSONVar clouds = metar["clouds"];
  String worstCover = "";
  if (JSON.typeof(clouds) == "array") {
    int n = clouds.length();
    for (int i = 0; i < n; i++) {
      JSONVar c = clouds[i];
      JSONVar coverVar = c["cover"];
      if (JSON.typeof(coverVar) != "string") continue;
      String cover = (const char*)coverVar;
      if (cover == "OVC" || cover == "BKN") { worstCover = "OVC"; break; }
      if (cover == "SCT" || cover == "FEW") worstCover = "SCT";
    }
  }
  if (worstCover == "OVC") return ICON_CLOUDY;
  if (worstCover == "SCT") return ICON_PARTLY_CLOUDY;
  return ICON_CLEAR;
}

bool fetchWeatherOnce() {
  Serial.println("=== fetchWeatherOnce() ===");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnect...");
    WiFi.reconnect(); // reuse saved credentials rather than re-launching the setup portal
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(300);
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Still not connected, aborting fetch.");
      return false;
    }
  }

  HTTPClient http;
  String url = "https://aviationweather.gov/api/data/metar?ids=" + currentStation + "&format=json";

  Serial.print("Requesting URL: ");
  Serial.println(url);

  http.begin(secureClient, url);
  http.addHeader("User-Agent", API_USER_AGENT);
  int httpCode = http.GET();
  Serial.printf("HTTP response code: %d\n", httpCode);

  if (httpCode == 204) {
    Serial.println("No current METAR data available for this station (204 No Content).");
    http.end();
    return false;
  }

  if (httpCode != 200) {
    Serial.println("Non-200 response, dumping body for debugging:");
    Serial.println(http.getString());
    http.end();
    return false;
  }

  // Arduino_JSON parses from a String (no stream/filter support like
  // ArduinoJson has), so pull the whole body first.
  String payload = http.getString();
  http.end();

  Serial.printf("Payload length: %d bytes\n", payload.length());
  Serial.println("First 200 chars of payload:");
  Serial.println(payload.substring(0, 200));

  JSONVar doc = JSON.parse(payload);

  if (JSON.typeof(doc) == "undefined") {
    Serial.println("JSON parse FAILED - JSON.parse returned undefined");
    return false;
  }
  Serial.println("JSON parsed OK");

  // Response is an array - grab the first (most recent) METAR entry.
  JSONVar metar = doc[0];
  if (JSON.typeof(metar) == "undefined") {
    Serial.println("No METAR entries in response for this station.");
    return false;
  }

  currentTemp = (float)(double)metar["temp"];
  float dewp  = (float)(double)metar["dewp"];
  currentHumidity = computeRelativeHumidity(currentTemp, dewp);

  double wspdKt = (double)metar["wspd"];
  currentWindSpeed = (float)(wspdKt * 1.852); // knots -> km/h

  currentPressure = (float)(double)metar["altim"]; // hPa

  JSONVar timeVar = metar["reportTime"];
  currentWeatherTime = (JSON.typeof(timeVar) == "string") ? (const char*)timeVar : "unknown";

  // Present weather (rain) - parsed from the raw METAR text rather than
  // wxString, since foreign stations like VVDN often report wxString as
  // null even when the raw text does contain a rain group.
  JSONVar rawVar = metar["rawOb"];
  String raw = (JSON.typeof(rawVar) == "string") ? String((const char*)rawVar) : "";
  int idx = raw.indexOf("RA");
  if (idx >= 0) {
    if (idx > 0 && raw[idx - 1] == '-') {
      currentRainCode = "Light Rain";
    } else if (idx > 0 && raw[idx - 1] == '+') {
      currentRainCode = "Heavy Rain";
    } else {
      currentRainCode = "Moderate Rain";
    }
  } else {
    currentRainCode = "No Rain";
  }

  currentWeatherIcon = metarToIcon(metar);

  Serial.printf("Current: %.1fC, %.0f%% RH (calc), wind=%.1fkm/h, %.0fhPa, rain=%s, time=%s\n",
                currentTemp, currentHumidity, currentWindSpeed, currentPressure,
                currentRainCode.c_str(),
                currentWeatherTime.c_str());

  return true;
}

// aviationweather.gov occasionally has transient hiccups under load -
// retry a couple of times with a short backoff before giving up, rather
// than leaving the screen stuck on stale/zeroed data for a full
// 30-minute cycle over what's usually a few-second blip.
bool fetchWeather() {
  for (int attempt = 1; attempt <= WEATHER_FETCH_MAX_ATTEMPTS; attempt++) {
    if (attempt > 1) {
      Serial.printf("Retrying weather fetch (attempt %d/%d)...\n", attempt, WEATHER_FETCH_MAX_ATTEMPTS);
      delay(3000 * (attempt - 1)); // 3s, then 6s
    }
    if (fetchWeatherOnce()) {
      weatherDataValid = true;
      checkKiteWindAlert();
      return true;
    }
  }
  Serial.printf("Weather fetch failed after %d attempts.\n", WEATHER_FETCH_MAX_ATTEMPTS);
  weatherDataValid = false;
  return false;
}

#endif // WEATHER_H
