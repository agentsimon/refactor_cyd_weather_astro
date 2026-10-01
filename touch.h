#ifndef TOUCH_H
#define TOUCH_H

#include "globals.h"

/*
  touch.h
  -------
  CST820 capacitive touch controller, read directly over I2C - no extra
  touch library required. See getTouchXY() for the calibration notes.
*/

// -----------------------------------------------------------------
// Diagnostic: scans the I2C bus and prints any responding addresses.
// Used to confirm the CST820 touch controller is actually reachable
// at CST820_ADDR (0x15) on the configured SDA/SCL pins.
// -----------------------------------------------------------------
void scanI2C() {
  Serial.println("=== I2C scan ===");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("  Device found at 0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  No I2C devices found - check TOUCH_SDA/TOUCH_SCL pin numbers.");
  }
  Serial.println("=== I2C scan done ===");
}

// -----------------------------------------------------------------
// Reads the CST820's finger-count register (0x02) over I2C.
// Uses a full stop between the register-address write and the
// data read (rather than a repeated start), since some touch
// controllers handle that more reliably. Returns 0xFF on I2C error.
// -----------------------------------------------------------------
uint8_t readTouchFingerCount() {
  Wire.beginTransmission(CST820_ADDR);
  Wire.write(0x02); // "finger number" register
  uint8_t err = Wire.endTransmission(true); // full stop
  if (err != 0) return 0xFF; // I2C error - controller not responding

  delay(1); // brief settle before the follow-up read

  Wire.requestFrom((int)CST820_ADDR, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0xFF;
}

bool isTouched() {
  uint8_t fingerNum = readTouchFingerCount();
  return fingerNum > 0 && fingerNum != 0xFF;
}

// -----------------------------------------------------------------
// Reads a touch position from the CST820, starting at register 0x01
// (gesture ID) through 0x06 (Y low byte) in one 6-byte block read.
// Returns true and fills x,y (in SCREEN coordinates, 0-319 x 0-239)
// if a finger is currently down.
//
// CALIBRATION: the CST820 reports raw coordinates in the panel's
// native PORTRAIT orientation (240 x 320), not adjusted for
// setRotation(1) landscape mode - so the axes are swapped, and one
// is inverted, relative to the rotated screen. Direction was
// confirmed by tapping the two screen corners:
//   top-left  (screen 0,0)     -> raw x=207, y=31
//   bottom-right (screen 319,239) -> raw x=76,  y=149
// This confirms screenX derives from raw Y (increasing), and screenY
// derives from raw X (inversely). However, using those exact tapped
// values as the calibration RANGE was too narrow - human taps rarely
// land on the literal edge pixel, so ordinary mid-screen taps were
// saturating past them and clamping to the screen edges. Using the
// panel's full native resolution (0-239 raw X, 0-319 raw Y) as the
// range instead gives proper full-screen coverage.
// -----------------------------------------------------------------
bool getTouchXY(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(CST820_ADDR);
  Wire.write(0x01);
  uint8_t err = Wire.endTransmission(true); // full stop
  if (err != 0) return false;

  delay(1);

  Wire.requestFrom((int)CST820_ADDR, 6);
  if (Wire.available() < 6) return false;

  Wire.read();                 // gesture ID, unused
  uint8_t fingerNum = Wire.read();
  uint8_t xH = Wire.read();
  uint8_t xL = Wire.read();
  uint8_t yH = Wire.read();
  uint8_t yL = Wire.read();

  if (fingerNum == 0) return false;

  uint16_t rawX = ((xH & 0x0F) << 8) | xL;
  uint16_t rawY = ((yH & 0x0F) << 8) | yL;

  // screenX increases with rawY; screenY increases as rawX DEcreases (inverted)
  long sx = map(rawY, 0, TOUCH_NATIVE_Y_MAX, 0, tft.width() - 1);
  long sy = map(rawX, TOUCH_NATIVE_X_MAX, 0, 0, tft.height() - 1);

  x = constrain(sx, 0, tft.width() - 1);
  y = constrain(sy, 0, tft.height() - 1);
  return true;
}

#endif // TOUCH_H
