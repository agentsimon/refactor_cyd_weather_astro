#ifndef LOCATION_H
#define LOCATION_H

#include "globals.h"

/*
  location.h
  ----------
  METAR station persistence (NVS via Preferences) and the tiny web form
  used to set it. Runs entirely in the background via locationServer -
  visit http://<board-ip>/ any time after WiFi connects.
*/

// Reads the saved station ID from flash, falling back to secrets.h's
// default if nothing has been saved yet (e.g. first boot).
void loadLocation() {
  prefs.begin("weatherapp", true); // read-only
  currentStation = prefs.getString("station", DEFAULT_METAR_STATION);
  prefs.end();
  currentStation.toUpperCase();
  Serial.printf("Loaded METAR station: %s\n", currentStation.c_str());
}

// Validates and persists a new station ID. Returns false (and saves
// nothing) if it isn't a plausible 3-4 character alphanumeric ICAO code.
bool saveStation(const String& stationRaw) {
  String station = stationRaw;
  station.trim();
  station.toUpperCase();

  bool valid = (station.length() >= 3 && station.length() <= 4);
  for (unsigned int i = 0; valid && i < station.length(); i++) {
    if (!isAlphaNumeric(station[i])) valid = false;
  }
  if (!valid) {
    Serial.printf("Rejected invalid station id: '%s'\n", stationRaw.c_str());
    return false;
  }

  prefs.begin("weatherapp", false);
  prefs.putString("station", station);
  prefs.end();

  currentStation = station;
  Serial.printf("Saved new METAR station: %s\n", currentStation.c_str());
  return true;
}

// GET / - shows a small form pre-filled with the current station.
void handleLocationRoot() {
  String html =
    "<html><head><title>CYD Weather - Station</title>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 16px;}"
    "label{font-weight:bold;} input{width:100%;padding:8px;margin:6px 0 16px;"
    "font-size:16px;box-sizing:border-box;text-transform:uppercase;} button{padding:10px 20px;font-size:16px;}"
    "</style></head><body>"
    "<h2>Set METAR Station</h2>"
    "<p>Enter the 4-letter ICAO airport code (e.g. VVDN for Da Nang).</p>"
    "<form action='/save' method='POST'>"
    "<label>Station ID</label>"
    "<input type='text' name='station' value='" + currentStation + "' maxlength='4'>"
    "<button type='submit'>Save</button>"
    "</form></body></html>";
  locationServer.send(200, "text/html", html);
}

// POST /save - validates, persists, and immediately refetches weather
// for the new station so the change shows up without waiting for the
// next 30-minute refresh cycle.
void handleLocationSave() {
  if (!locationServer.hasArg("station")) {
    locationServer.send(400, "text/plain", "Missing station");
    return;
  }

  String station = locationServer.arg("station");

  if (!saveStation(station)) {
    locationServer.send(400, "text/html",
      "<html><body><h3>Invalid station ID.</h3>"
      "<p>Enter a 3-4 letter/number ICAO airport code.</p>"
      "<a href='/'>Back</a></body></html>");
    return;
  }

  locationServer.send(200, "text/html",
    "<html><body><h3>Station saved!</h3>"
    "<p>Station: " + currentStation + "</p>"
    "<p>Refetching weather for the new station now...</p>"
    "<a href='/'>Back</a></body></html>");

  bool ok = fetchWeather();
  lastFetch = millis();
  Serial.println(ok ? ">>> Refetch after station change OK." : ">>> Refetch after station change failed.");
  if (currentScreen != SCREEN_MENU) {
    redrawCurrentScreen();
  }
}

#endif // LOCATION_H
