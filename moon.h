#ifndef MOON_H
#define MOON_H

#include "globals.h"

/*
  moon.h
  ------
  Moon phase (for the main menu icon). Phase is computed entirely
  locally from the board's NTP-synced clock - no API call needed, so
  it's always available and free to recompute any time. It's
  independent of the Ephemeris API/its daily call cap.

  The Moon's zodiac SIGN, on the other hand, does need the Ephemeris
  API (it requires actual orbital position, not just elapsed time), so
  the menu just reads it from ephemerisSign[1] ("Moon" is always index
  1 in EPHEMERIS_BODIES, in display.h) - whatever the last successful
  fetchEphemeris() call found. The sign only changes every ~2.3 days,
  so it's refreshed on its own slower cycle (MOON_SIGN_REFRESH_INTERVAL_MS
  in the main .ino's loop()) rather than piggybacking on the 30-min
  weather cycle, to stay well clear of the Ephemeris API's daily cap.
*/

const char* MOON_PHASE_NAMES[8] = {
  "New Moon", "Waxing Crescent", "First Quarter", "Waxing Gibbous",
  "Full Moon", "Waning Gibbous", "Last Quarter", "Waning Crescent"
};

// Returns the Moon's age in days (0-29.53, 0=new, ~14.77=full) based on
// the current UTC time, and writes the matching phase name. Returns
// false (and leaves both outputs untouched) if NTP time isn't synced yet.
bool computeMoonPhase(float &outAgeDays, const char* &outPhaseName) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 200)) return false; // short timeout - this gets called on every menu draw

  time_t now = mktime(&timeinfo); // NOTE: mktime treats tm as UTC here since our clock IS UTC (see configTime(0,0,...) in setup)
  double daysSinceRef = (double)(now - (time_t)MOON_REF_EPOCH) / 86400.0;
  double age = fmod(daysSinceRef, MOON_SYNODIC_DAYS);
  if (age < 0) age += MOON_SYNODIC_DAYS;

  int phaseIndex = ((int)round(age / (MOON_SYNODIC_DAYS / 8.0))) % 8;

  outAgeDays = (float)age;
  outPhaseName = MOON_PHASE_NAMES[phaseIndex];
  return true;
}

// Draws a small moon-phase icon centered at (cx, cy) with radius r,
// shading the illuminated portion via the standard "terminator ellipse"
// approximation (good enough for an icon this size; not full lunar theory).
void drawMoonPhaseIcon(int cx, int cy, int r, float ageDays) {
  float phaseFrac = ageDays / MOON_SYNODIC_DAYS;   // 0..1, 0=new, 0.5=full
  float phaseAngle = phaseFrac * 2.0 * PI;          // 0=new, PI=full, 2PI=new again
  bool waxing = (phaseFrac < 0.5);
  float k = cos(phaseAngle);

  tft.fillCircle(cx, cy, r, tft.color565(25, 25, 35)); // unlit disk (dim, not pure black - stays visible against the black background)
  tft.drawCircle(cx, cy, r, TFT_DARKGREY);

  for (int y = -r; y <= r; y++) {
    int halfW = (int)sqrt((float)(r * r - y * y));
    if (halfW <= 0) continue;
    int xLeft  = cx - halfW;
    int xRight = cx + halfW;
    int termOffset = (int)(k * halfW);

    int litLeft, litRight;
    if (waxing) {
      litLeft  = cx + termOffset;
      litRight = xRight;
    } else {
      litLeft  = xLeft;
      litRight = cx - termOffset;
    }
    if (litLeft < xLeft)   litLeft = xLeft;
    if (litRight > xRight) litRight = xRight;
    if (litRight >= litLeft) {
      tft.drawFastHLine(litLeft, cy + y, litRight - litLeft + 1, TFT_WHITE);
    }
  }
}

#endif // MOON_H
