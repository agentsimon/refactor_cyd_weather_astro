#ifndef DISPLAY_H
#define DISPLAY_H

#include "globals.h"

/*
  display.h
  ---------
  Everything that draws to the TFT: the main menu, the Current
  Conditions and Ephemeris screens, and the small vector weather icons.
  No network or sensor logic lives here - it only reads the shared
  state declared in globals.h.
*/

// Forward declarations for functions used before their definition below.
// Headers don't get Arduino's automatic prototype generation (that only
// applies to code written directly in the .ino), so these are declared
// by hand rather than relying on file order.
void drawWeatherIcon(int cx, int cy, int size, WeatherIconType icon);
void drawSun(int cx, int cy, int size);
void drawCloud(int cx, int cy, int size, uint16_t color);
void drawRainDrops(int cx, int cy, int size);
void drawSnowflakes(int cx, int cy, int size);
void drawFogLines(int cx, int cy, int size);
void drawLightningBolt(int cx, int cy, int size);
void drawEphemeris();
void drawCurrentConditions();

void screenTest() {
  Serial.println("=== Screen self-test ===");
  Serial.printf("tft.width()=%d tft.height()=%d\n", tft.width(), tft.height());

  uint16_t barColors[] = { TFT_BLACK, TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE };
  int numBars = 5;
  int barWidth = tft.width() / numBars;

  for (int i = 0; i < numBars; i++) {
    tft.fillRect(i * barWidth, 0, barWidth, tft.height(), barColors[i]);
  }

  Serial.println("  Showing black/red/green/blue/white bars");
  delay(2000);
  Serial.println("=== Screen self-test complete ===");
}

void drawMenu() {
  tft.fillScreen(TFT_BLACK);
  int rowHeight = tft.height() / 2;

  for (int i = 0; i < 2; i++) {
    int y = i * rowHeight;
    tft.drawRect(0, y, tft.width(), rowHeight, TFT_WHITE);
    tft.setTextColor(MENU_COLORS[i], TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(15, y + rowHeight / 2 - 8);
    tft.println(MENU_LABELS[i]);
  }

  // Weather icon next to "Current Weather" (row 0)
  if (weatherDataValid) {
    drawWeatherIcon(255, rowHeight / 2, 30, currentWeatherIcon);
  }

  // Moon phase icon + position next to "Ephemeris" (row 1)
  float moonAge;
  const char* moonPhaseName;
  if (computeMoonPhase(moonAge, moonPhaseName)) {
    int row1CenterY = rowHeight + rowHeight / 2;
    drawMoonPhaseIcon(255, row1CenterY - 10, 26, moonAge);

    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(195, row1CenterY + 25);
    if (ephemerisCount > 1) {
      // Same degree/sign/minute format as the full Ephemeris screen.
      int wholeDeg = (int)ephemerisDegree[1];
      int minutes = (int)round((ephemerisDegree[1] - wholeDeg) * 60.0);
      if (minutes >= 60) { // rounding can push e.g. 10.999 -> 11d60' - carry it
        minutes = 0;
        wholeDeg++;
      }
      tft.printf("%2d %s %02d", wholeDeg, ephemerisSign[1].c_str(), minutes);
    } else {
      tft.print("--");
    }
  }

  // Current time/date in the top-left corner - same Da Nang local-time
  // conversion used on the Current Conditions screen.
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 200)) {
    time_t utcEpoch = mktime(&timeinfo);
    time_t localEpoch = utcEpoch + (time_t)DANANG_UTC_OFFSET_MIN * 60;
    struct tm localTm;
    gmtime_r(&localEpoch, &localTm);

    tft.setTextSize(2);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(15, 76);
    tft.printf("%02d:%02d  %02d-%02d-%04d",
                localTm.tm_hour, localTm.tm_min,
                localTm.tm_mday, localTm.tm_mon + 1, localTm.tm_year + 1900);
  }
}

// Maps a touch Y coordinate to one of the 2 menu rows (0-1)
int menuRowFromY(uint16_t y) {
  int rowHeight = tft.height() / 2;
  int row = y / rowHeight;
  if (row < 0) row = 0;
  if (row > 1) row = 1;
  return row;
}

// Switches to the screen for the selected menu row and draws it
void selectMenuRow(int row) {
  switch (row) {
    case 0: currentScreen = SCREEN_CURRENT;   break;
    case 1: currentScreen = SCREEN_EPHEMERIS; break;
  }

  if (currentScreen == SCREEN_EPHEMERIS) {
    // Live snapshot for "right now" - fetch on every visit rather than
    // relying on the 30-minute weather refetch cycle. Show a brief
    // loading message first since the request can take a second or two.
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Fetching ephemeris...");
    ephemerisValid = fetchEphemeris();
  }

  redrawCurrentScreen();
}

// Dispatches to the right draw function for whatever currentScreen is
void redrawCurrentScreen() {
  switch (currentScreen) {
    case SCREEN_EPHEMERIS: drawEphemeris();            break;
    case SCREEN_CURRENT:   drawCurrentConditions();    break;
    case SCREEN_MENU:      drawMenu();                 break;
  }
}

void drawEphemeris() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 4);
  tft.println("Ephemeris");

  if (ephemerisLimitReached) {
    tft.setTextSize(1);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setCursor(10, 26);
    tft.println("Daily API limit reached");
    tft.setCursor(10, 40);
    tft.printf("(%d/%d calls used today)", ephemerisCallsToday, EPHEMERIS_MAX_CALLS_PER_DAY);
    tft.setCursor(10, 54);
    tft.println("Resets at UTC midnight.");
    return;
  }

  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 26);
  tft.println(ephemerisTimeLabel + " UTC");

  if (!ephemerisValid || ephemerisCount == 0) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 60);
    tft.println("No data available");
    return;
  }

  int y = 42;
  int rowHeight = 17;
  tft.setTextSize(2);
  for (int i = 0; i < ephemerisCount; i++) {
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.setCursor(10, y);
    tft.print(ephemerisName[i]);

    // The Moon (index 1) can't actually be retrograde - that's an
    // apparent-motion effect from Earth and another body orbiting the Sun
    // at different speeds, which doesn't apply to a body orbiting Earth
    // directly. Ignore whatever the API says for it, just in case.
    bool showRetro = ephemerisRetro[i] && (i != 1);

    int wholeDeg = (int)ephemerisDegree[i];
    int minutes = (int)round((ephemerisDegree[i] - wholeDeg) * 60.0);
    if (minutes >= 60) { // rounding can push e.g. 10.999 -> 11d60' - carry it
      minutes = 0;
      wholeDeg++;
    }

    char posBuf[24];
    snprintf(posBuf, sizeof(posBuf), "%2d %s %02d%s", wholeDeg,
             ephemerisSign[i].c_str(), minutes, showRetro ? " Rx" : "");

    tft.setTextColor(showRetro ? TFT_LIGHT_RED : TFT_WHITE, TFT_BLACK);
    tft.setCursor(130, y);
    tft.println(posBuf);

    y += rowHeight;
  }

  // Small remaining-calls readout at the bottom so it's easy to see how
  // close the daily cap is without checking the Serial log.
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(10, y + 2);
  tft.printf("%d/%d calls used today", ephemerisCallsToday, EPHEMERIS_MAX_CALLS_PER_DAY);
}

void drawCurrentConditions() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 6);
  tft.println("Current Conditions");

  if (!weatherDataValid) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 60);
    tft.println("No data available");
    tft.setTextSize(1);
    tft.setCursor(10, 90);
    tft.println("Weather fetch failed - will retry");
    tft.setCursor(10, 104);
    tft.println("automatically in the background.");
    return;
  }

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(4);
  tft.setCursor(20, 40);
  tft.printf("%.1fC", currentTemp);

  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(20, 90);
  tft.printf("%-10s%.0f %%", "Humidity:", currentHumidity);

  tft.setCursor(20, 115);
  tft.printf("%-10s%s", "Rain:", currentRainCode.c_str());

  tft.setCursor(20, 140);
  tft.printf("%-10s%.0f hPa", "Pressure:", currentPressure);

  tft.setCursor(20, 165);
  tft.printf("%-10s%.1f km/h", "Wind:", currentWindSpeed);

  // Current time at the bottom of the screen - the board's NTP clock,
  // converted from UTC to Da Nang local time using the offset from
  // secrets.h. Reflects the moment this screen was drawn/refreshed,
  // not a live-ticking clock (the screen doesn't redraw every second).
struct tm timeinfo;
if (getLocalTime(&timeinfo, 200)) {
  time_t utcEpoch = mktime(&timeinfo);
  time_t localEpoch = utcEpoch + (time_t)DANANG_UTC_OFFSET_MIN * 60;
  struct tm localTm;
  gmtime_r(&localEpoch, &localTm);

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(20, 218);
  tft.printf("Time: %02d:%02d %02d-%02d-%04d",
              localTm.tm_hour, localTm.tm_min,
              localTm.tm_mday, localTm.tm_mon + 1, localTm.tm_year + 1900);
}

  drawWeatherIcon(265, 60, 40, currentWeatherIcon);
}

// -----------------------------------------------------------------
// Draws a simple vector weather icon centered at (cx, cy) with the
// given size (roughly the icon's radius in pixels). No image/bitmap
// assets needed - just basic shapes.
// -----------------------------------------------------------------
void drawWeatherIcon(int cx, int cy, int size, WeatherIconType icon) {
  switch (icon) {
    case ICON_CLEAR:
      drawSun(cx, cy, size);
      break;

    case ICON_PARTLY_CLOUDY:
      drawSun(cx - size / 3, cy - size / 4, size * 2 / 3);
      drawCloud(cx + size / 4, cy + size / 4, size, TFT_LIGHTGREY);
      break;

    case ICON_CLOUDY:
      drawCloud(cx, cy, size, TFT_LIGHTGREY);
      break;

    case ICON_FOG:
      drawCloud(cx, cy - size / 4, size * 3 / 4, TFT_DARKGREY);
      drawFogLines(cx, cy + size / 3, size);
      break;

    case ICON_RAIN:
      drawCloud(cx, cy - size / 4, size * 3 / 4, TFT_LIGHTGREY);
      drawRainDrops(cx, cy + size / 3, size);
      break;

    case ICON_SNOW:
      drawCloud(cx, cy - size / 4, size * 3 / 4, TFT_LIGHTGREY);
      drawSnowflakes(cx, cy + size / 3, size);
      break;

    case ICON_THUNDERSTORM:
      drawCloud(cx, cy - size / 4, size * 3 / 4, TFT_DARKGREY);
      drawLightningBolt(cx, cy + size / 4, size);
      break;
  }
}

void drawSun(int cx, int cy, int size) {
  int r = size / 2;
  tft.fillCircle(cx, cy, r, TFT_YELLOW);
  // rays
  for (int angle = 0; angle < 360; angle += 45) {
    float rad = angle * PI / 180.0;
    int x1 = cx + (int)(cos(rad) * (r + 4));
    int y1 = cy + (int)(sin(rad) * (r + 4));
    int x2 = cx + (int)(cos(rad) * (r + 10));
    int y2 = cy + (int)(sin(rad) * (r + 10));
    tft.drawLine(x1, y1, x2, y2, TFT_YELLOW);
  }
}

void drawCloud(int cx, int cy, int size, uint16_t color) {
  int r = size / 3;
  tft.fillCircle(cx - r, cy, r, color);
  tft.fillCircle(cx, cy - r / 2, r * 6 / 5, color);
  tft.fillCircle(cx + r, cy, r, color);
  tft.fillRect(cx - r, cy, r * 2, r, color);
}

void drawRainDrops(int cx, int cy, int size) {
  int spacing = size / 3;
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * spacing;
    tft.drawLine(x, cy, x - 3, cy + 10, TFT_CYAN);
  }
}

void drawSnowflakes(int cx, int cy, int size) {
  int spacing = size / 3;
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * spacing;
    tft.drawLine(x - 4, cy + 4, x + 4, cy + 4, TFT_WHITE);
    tft.drawLine(x, cy, x, cy + 8, TFT_WHITE);
    tft.drawLine(x - 3, cy + 1, x + 3, cy + 7, TFT_WHITE);
    tft.drawLine(x + 3, cy + 1, x - 3, cy + 7, TFT_WHITE);
  }
}

void drawFogLines(int cx, int cy, int size) {
  int halfWidth = size / 2;
  for (int i = 0; i < 3; i++) {
    int y = cy + i * 6;
    tft.drawLine(cx - halfWidth, y, cx + halfWidth, y, TFT_LIGHTGREY);
  }
}

void drawLightningBolt(int cx, int cy, int size) {
  int s = size / 4;
  tft.fillTriangle(cx - s / 2, cy, cx + s / 2, cy, cx - s / 4, cy + s, TFT_YELLOW);
  tft.fillTriangle(cx - s / 4, cy + s, cx + s / 4, cy + s, cx, cy + s * 2, TFT_YELLOW);
}

#endif // DISPLAY_H
