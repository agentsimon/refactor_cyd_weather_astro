/*
  CYD (JC2432W328) - METAR Weather Display
  --------------------------------------------------
  Fetches current conditions for a METAR station (default: VVDN, Da Nang
  Airport) from aviationweather.gov's free, keyless Data API. Shows a
  main menu; touch "Current Weather" for temperature, wind, pressure,
  calculated humidity, rain condition, and the current time, or
  "Ephemeris" for planetary positions. Touch anywhere to return to the
  menu. Data refetches every 30 minutes in the background.

  SKETCH LAYOUT: split across multiple files in this same folder -
  Arduino compiles them all together as one sketch.
    globals.h    - shared includes, constants, enums, extern globals,
                    and cross-file function prototypes (read this first)
    touch.h      - CST820 capacitive touch (I2C)
    wifi_setup.h - WiFiManager captive-portal connection
    location.h   - METAR station persistence + on-board web config form
    weather.h    - fetches/parses the METAR from aviationweather.gov
    ephemeris.h  - fetches planetary positions from FreeAstroAPI
    moon.h       - local moon phase calculation + icon
    kite_alert.h - wind-threshold email alert (Gmail SMTP)
    display.h    - all TFT drawing (menu, screens, weather icons)
    secrets.h    - station default, API keys, email settings (fill in)
  This file just defines the shared global objects/variables and holds
  setup()/loop().

  NOTE ON METAR DATA: unlike Open-Meteo, METAR doesn't report a
  precipitation amount in mm as standard - only a present-weather group
  in the raw text (e.g. "-RA", "RA", "+RA"). This sketch turns that into
  a plain-language "No Rain" / "Light Rain" / "Moderate Rain" /
  "Heavy Rain" condition instead of a millimeter figure. Humidity isn't
  in METAR either - it's calculated from temperature and dew point.

  The menu's "Ephemeris" entry calls the FreeAstroAPI ephemeris endpoint
  (https://www.freeastroapi.com/docs/western/ephemeris) for a single
  snapshot - the planetary positions at the exact moment the entry is
  tapped - using the board's NTP-synced clock for the timestamp.
  Requires an EPHEMERIS_API_KEY in secrets.h.

  WiFi setup uses WiFiManager (captive portal) instead of hardcoded
  credentials: on first boot (or if saved credentials fail), the board
  starts an access point named "CYD-Setup". Connect a phone or laptop
  to it, a setup page should open automatically (or browse to the IP
  shown on screen), pick your WiFi network and enter the password.
  Credentials are then saved to flash for future boots.

  Hold your finger on the touchscreen for the first 2 seconds after
  power-on to erase saved WiFi credentials and force the setup portal
  again (e.g. to switch networks). Touch is read directly over I2C
  (CST820 controller) - no extra touch library required.

  Libraries required (Arduino IDE > Tools > Manage Libraries):
    - TFT_eSPI      (already configured for this board via User_Setup.h)
    - Arduino_JSON  (by Arduino, v0.2.x)
    - WiFiManager   (by tzapu)
    - ESP Mail Client (by Mobizt)

  Put all the .h files AND secrets.h in the SAME FOLDER as this .ino
  file. WiFi credentials are entered via the setup portal and saved to
  flash, not stored in secrets.h - see secrets.h for what it should
  actually contain.

  To change the METAR station, visit http://<board-ip>/ in a browser
  while the board is on your network and enter a new ICAO airport code.
*/

#include "globals.h"
#include "touch.h"
#include "wifi_setup.h"
#include "location.h"
#include "weather.h"
#include "ephemeris.h"
#include "moon.h"
#include "kite_alert.h"
#include "display.h"

// ---------- Hardware / library objects ----------
TFT_eSPI tft = TFT_eSPI();
WiFiManager wifiManager;
Preferences prefs;
WebServer   locationServer(80);
SMTPSession smtp;
WiFiClientSecure secureClient; // shared TLS client for the METAR + Ephemeris HTTPS calls

// ---------- Weather state ----------
float currentTemp = 0;
float currentHumidity = 0;
String currentRainCode = "No Rain";
float currentPressure = 0;
float currentWindSpeed = 0;
WeatherIconType currentWeatherIcon = ICON_CLEAR;
String currentWeatherTime = "";
bool  weatherDataValid = false;

// ---------- Ephemeris state ----------
const char* EPHEMERIS_BODIES[NUM_EPHEMERIS_BODIES] = {
  "Sun", "Moon", "Mercury", "Venus", "Mars",
  "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto"
};
String ephemerisName[NUM_EPHEMERIS_BODIES];
String ephemerisSign[NUM_EPHEMERIS_BODIES];
float  ephemerisDegree[NUM_EPHEMERIS_BODIES];
bool   ephemerisRetro[NUM_EPHEMERIS_BODIES];
int    ephemerisCount = 0;
bool   ephemerisValid = false;
String ephemerisTimeLabel = "";
int  ephemerisCallsToday = 0;
int  ephemerisCallDayKey = -1;
bool ephemerisLimitReached = false;

// ---------- Menu / screen state ----------
AppScreen currentScreen = SCREEN_MENU;
const char* MENU_LABELS[2] = {
  "Current Weather", "Ephemeris"
};
const uint16_t MENU_COLORS[2] = {
  TFT_YELLOW, TFT_MAGENTA
};
bool wasTouched = false;
unsigned long lastActivityTime = 0;
bool screenBlanked = false;
unsigned long lastFetch = 0;
unsigned long lastMoonSignFetch = 0;

// ---------- METAR station / location ----------
String currentStation = DEFAULT_METAR_STATION;

// ---------- NTP servers ----------
// Three servers so a single slow/blocked one doesn't stall the sync -
// the ESP32 core tries them in order.
const char* NTP_SERVER1 = "pool.ntp.org";
const char* NTP_SERVER2 = "time.google.com";
const char* NTP_SERVER3 = "time.cloudflare.com";

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  scanI2C(); // diagnostic: lists any I2C devices found, to verify CST820 wiring

  tft.init();
  tft.setRotation(1);

  screenTest();

  // Give a 2-second window right at boot to touch the screen and
  // force a WiFi credentials reset (equivalent to the old BOOT-button hold).
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Touch screen now to");
  tft.setCursor(10, 35);
  tft.println("reset WiFi setup...");

  bool resetRequested = false;
  unsigned long touchWindowStart = millis();
  int checkCount = 0;
  while (millis() - touchWindowStart < 2000) {
    uint8_t fingerNum = readTouchFingerCount();
    checkCount++;
    if (checkCount % 5 == 0) { // print every ~250ms so we can see it's alive without flooding
      Serial.print("Touch check, fingerNum=");
      Serial.println(fingerNum);
    }
    if (fingerNum > 0 && fingerNum != 0xFF) {
      resetRequested = true;
      break;
    }
    delay(50);
  }

  if (resetRequested) {
    Serial.println("Touch detected at boot - erasing saved WiFi credentials.");
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(10, 10);
    tft.println("Resetting WiFi setup...");
    wifiManager.resetSettings();
    delay(500);
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Connecting WiFi...");

  connectWiFi();

  // Skip certificate validation for the shared HTTPS client - fine for a
  // hobby project reading two public, non-sensitive APIs. Without this,
  // ESP32's HTTPClient fails the TLS handshake silently on https:// URLs
  // (empty body, no useful HTTP status), which is what was happening here.
  secureClient.setInsecure();

  loadLocation();
  locationServer.on("/", handleLocationRoot);
  locationServer.on("/save", HTTP_POST, handleLocationSave);
  locationServer.begin();
  Serial.println("Location config server started on port 80.");

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("WiFi connected!");
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 40);
  tft.println("To set your station, visit:");
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 55);
  tft.print("http://");
  tft.println(WiFi.localIP());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 75);
  tft.printf("Station: %s\n", currentStation.c_str());
  delay(4000);

  Serial.println("Syncing time via NTP (needed for Ephemeris timestamps)...");
  configTime(0, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3); // UTC, no DST offset - API wants UTC

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Syncing time...");

  // configTime() only *starts* the sync in the background - it doesn't wait
  // for it. Actively poll for up to ~15s so we know (and log) whether it
  // actually succeeded, rather than silently sailing on with no time set.
  struct tm timeinfo;
  bool timeSynced = false;
  unsigned long ntpStart = millis();
  while (millis() - ntpStart < 15000) {
    if (getLocalTime(&timeinfo, 1000)) {
      timeSynced = true;
      break;
    }
    Serial.println("  ...waiting for NTP sync");
  }

  if (timeSynced) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.printf("NTP sync OK: %s UTC\n", buf);
  } else {
    Serial.println("NTP sync FAILED after 15s - Ephemeris won't work until it");
    Serial.println("catches up in the background (or check network/firewall,");
    Serial.println("NTP needs outbound UDP port 123).");
    tft.setTextSize(1);
    tft.setCursor(10, 40);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.println("Time sync failed - Ephemeris");
    tft.println("may not work yet. Continuing...");
    delay(2000);
  }

  bool ok = fetchWeather();
  lastFetch = millis();
  Serial.println(ok ? ">>> Initial fetch OK." : ">>> Initial fetch failed.");

  Serial.println("Fetching initial Moon sign for the menu icon...");
  bool moonOk = fetchEphemeris();
  lastMoonSignFetch = millis();
  Serial.println(moonOk ? ">>> Initial moon sign fetch OK." : ">>> Initial moon sign fetch failed - will retry in the background.");

  currentScreen = SCREEN_MENU;
  drawMenu();
  lastActivityTime = millis();
}

void loop() {
  locationServer.handleClient(); // serves the station-config page at any time, not just at boot

  if (millis() - lastFetch > REFRESH_INTERVAL_MS ||
      (!weatherDataValid && millis() - lastFetch > RETRY_INTERVAL_MS)) {
    Serial.println("\n=== Periodic refetch ===");
    bool ok = fetchWeather();
    lastFetch = millis();
    Serial.println(ok ? ">>> Refetch OK." : ">>> Refetch failed, will retry sooner than the usual 30 min.");
    if (currentScreen != SCREEN_MENU && !screenBlanked) {
      redrawCurrentScreen(); // refresh whatever's currently on screen with the new data
    }
  }

  if (millis() - lastMoonSignFetch > MOON_SIGN_REFRESH_INTERVAL_MS) {
    // Only refreshes ephemerisSign[1] (Moon) for the menu icon - the full
    // Ephemeris screen still fetches its own always-fresh snapshot on tap,
    // independent of this. Runs silently in the background (no on-screen
    // "fetching" message) since it doesn't disturb whatever's on screen.
    Serial.println("\n=== Periodic Ephemeris refresh (moon sign for menu) ===");
    bool ok = fetchEphemeris();
    lastMoonSignFetch = millis();
    Serial.println(ok ? ">>> Moon sign refresh OK." : ">>> Moon sign refresh failed - will retry next cycle.");
    if (currentScreen == SCREEN_MENU && !screenBlanked) {
      drawMenu(); // refresh the icon if we're sitting on the menu right now
    }
  }

  uint16_t touchX, touchY;
  bool touchedNow = getTouchXY(touchX, touchY);
  bool pressEdge = touchedNow && !wasTouched; // only act on a NEW touch-down, not a held one
  wasTouched = touchedNow;

  if (pressEdge) {
    lastActivityTime = millis();

    if (screenBlanked) {
      // Wake up: any touch while blanked just wakes the display and
      // shows the menu - it doesn't also count as a menu selection.
      Serial.println("Touch detected - waking display.");
      screenBlanked = false;
      currentScreen = SCREEN_MENU;
      drawMenu();
    } else {
      Serial.print("Touch (screen coords): x=");
      Serial.print(touchX);
      Serial.print(" y=");
      Serial.println(touchY);
      if (currentScreen == SCREEN_MENU) {
        selectMenuRow(menuRowFromY(touchY));
      } else {
        currentScreen = SCREEN_MENU;
        drawMenu();
      }
    }
  }

  if (!screenBlanked && millis() - lastActivityTime > SCREEN_TIMEOUT_MS) {
    // NOTE: this board's backlight (GPIO21) doesn't actually respond to
    // software control on this unit (confirmed via isolated pin test) -
    // it's likely hard-wired always-on with no switching transistor.
    // Blanking the screen CONTENT to solid black is the fallback
    // burn-in defense: no static bright shapes sit in one place for
    // hours, even though the backlight itself stays lit throughout.
    Serial.println("No activity for 30s - blanking screen content.");
    tft.fillScreen(TFT_BLACK);
    screenBlanked = true;
  }

  delay(50);
}
