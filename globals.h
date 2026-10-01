#ifndef GLOBALS_H
#define GLOBALS_H

/*
  globals.h
  ---------
  Shared includes, pin/timing constants, enums, global object/state
  declarations (as `extern`), and forward declarations for functions
  that get called from a .h file other than the one they're defined in.

  Every other header in this sketch includes ONLY this file (never each
  other), so #include order in the main .ino never matters.

  The actual variable/object DEFINITIONS live in the main .ino
  (cyd_metar_weather.ino) - this file only declares them.
*/

// NOTE ON INCLUDE ORDER: Arduino_JSON's JSONVar.h #defines "typeof" as
// "typeof_" internally, which breaks the ESP32 core's own gpio_ll.h if
// that header hasn't been parsed yet. TFT_eSPI.h pulls in gpio_ll.h, so
// it MUST be included before Arduino_JSON.h below - don't reorder these.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <Arduino_JSON.h>
#include <Wire.h>
#include <time.h>
#include <math.h>
#include <Preferences.h>
#include <WebServer.h>
#include <ESP_Mail_Client.h> // by Mobizt - install via Library Manager
#include "secrets.h"

// ---------- Hardware / library objects (defined in the main .ino) ----------
extern TFT_eSPI tft;
extern WiFiManager wifiManager;
extern Preferences prefs;
extern WebServer locationServer;
extern SMTPSession smtp;
extern WiFiClientSecure secureClient; // shared TLS client for the METAR + Ephemeris HTTPS calls

// ---------- Pins ----------
#define BACKLIGHT_PIN 21
#define TOUCH_SDA 33
#define TOUCH_SCL 32
#define CST820_ADDR 0x15
#define TOUCH_NATIVE_X_MAX 239 // native panel short axis (portrait width)
#define TOUCH_NATIVE_Y_MAX 319 // native panel long axis (portrait height)

// ---------- Timing ----------
#define REFRESH_INTERVAL_MS (30UL * 60UL * 1000UL)      // refetch weather every 30 min
#define RETRY_INTERVAL_MS   (2UL * 60UL * 1000UL)       // retry sooner if we have no valid data yet
#define SCREEN_TIMEOUT_MS   (30UL * 1000UL)              // blank after 30s of no touches
#define MOON_SYNODIC_DAYS 29.530588
#define MOON_REF_EPOCH 947182440UL // a known New Moon: 2000-01-06 18:14 UTC
#define MOON_SIGN_REFRESH_INTERVAL_MS (4UL * 60UL * 60UL * 1000UL) // every 4 hours
#define NUM_EPHEMERIS_BODIES 10
#define EPHEMERIS_MAX_CALLS_PER_DAY 60
#define WEATHER_FETCH_MAX_ATTEMPTS 3
#define KITE_MIN_WIND_KMH 8.0

// A lighter/brighter red than TFT_eSPI's built-in TFT_RED (0xF800), which
// reads too dark against black at small font sizes. RGB(255,90,90) in RGB565.
#define TFT_LIGHT_RED 0xFACB

// ---------- Enums ----------
enum WeatherIconType {
  ICON_CLEAR,
  ICON_PARTLY_CLOUDY,
  ICON_CLOUDY,
  ICON_FOG,
  ICON_RAIN,
  ICON_SNOW,
  ICON_THUNDERSTORM
};

enum AppScreen {
  SCREEN_MENU,
  SCREEN_EPHEMERIS,
  SCREEN_CURRENT
};

// ---------- Weather state ----------
extern float currentTemp;
extern float currentHumidity;      // derived from temp/dewpoint - METAR doesn't report RH directly
extern String currentRainCode;     // human-readable rain condition parsed from the raw METAR text
extern float currentPressure;
extern float currentWindSpeed;     // km/h (converted from the METAR's knots)
extern WeatherIconType currentWeatherIcon;
extern String currentWeatherTime;  // the METAR's own reportTime string
extern bool  weatherDataValid;     // false until the first successful fetch

// ---------- Ephemeris state ----------
extern const char* EPHEMERIS_BODIES[NUM_EPHEMERIS_BODIES];
extern String ephemerisName[NUM_EPHEMERIS_BODIES];
extern String ephemerisSign[NUM_EPHEMERIS_BODIES];
extern float  ephemerisDegree[NUM_EPHEMERIS_BODIES];
extern bool   ephemerisRetro[NUM_EPHEMERIS_BODIES];
extern int    ephemerisCount;
extern bool   ephemerisValid;
extern String ephemerisTimeLabel; // human-readable UTC timestamp for the snapshot shown
extern int  ephemerisCallsToday;
extern int  ephemerisCallDayKey;  // encodes the UTC date the counter applies to
extern bool ephemerisLimitReached;

// ---------- Menu / screen state ----------
extern AppScreen currentScreen;
extern const char* MENU_LABELS[2];
extern const uint16_t MENU_COLORS[2];
extern bool wasTouched;            // for edge-detecting a new touch-down, not a held touch
extern unsigned long lastActivityTime;
extern bool screenBlanked;
extern unsigned long lastFetch;
extern unsigned long lastMoonSignFetch;

// ---------- METAR station / location ----------
extern String currentStation;

// ---------- NTP servers ----------
extern const char* NTP_SERVER1;
extern const char* NTP_SERVER2;
extern const char* NTP_SERVER3;

// ---------- Cross-file function prototypes ----------
// Every .h in this sketch only includes globals.h (never each other), so
// any function called from a DIFFERENT .h than the one that defines it
// needs its prototype declared here.
bool fetchWeather();                 // weather.h    - called from location.h, main .ino
bool fetchEphemeris();               // ephemeris.h  - called from display.h, main .ino
void checkKiteWindAlert();           // kite_alert.h - called from weather.h
bool computeMoonPhase(float &outAgeDays, const char* &outPhaseName); // moon.h - called from display.h
void drawMoonPhaseIcon(int cx, int cy, int r, float ageDays);        // moon.h - called from display.h
void drawMenu();                     // display.h    - called from main .ino
void redrawCurrentScreen();          // display.h    - called from location.h, main .ino

#endif // GLOBALS_H
