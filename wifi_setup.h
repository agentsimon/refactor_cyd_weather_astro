#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include "globals.h"

/*
  wifi_setup.h
  ------------
  WiFiManager (captive portal) connection logic. On first boot (or if
  saved credentials fail), the board starts an access point named
  "CYD-Setup" so the user can pick their WiFi network from a phone or
  laptop instead of hardcoding credentials.
*/

// Fires as soon as the user submits credentials in the captive portal,
// right before WiFiManager attempts to connect with them - gives us a
// chance to show something other than the static instructions screen.
void saveConfigCallback() {
  Serial.println("New WiFi credentials submitted - attempting to connect...");
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Connecting to");
  tft.println("your network...");
  tft.setTextSize(1);
  tft.setCursor(10, 60);
  tft.println("This may take a few seconds.");
}

// Shown on the TFT while the WiFiManager config portal is active,
// i.e. while waiting for the user to connect and pick a network.
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered WiFi config mode.");
  Serial.print("Config portal SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("Config portal IP: ");
  Serial.println(WiFi.softAPIP());

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.println("WiFi Setup Needed");

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 45);
  tft.println("1. Connect your phone/laptop to:");
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(20, 60);
  tft.println(myWiFiManager->getConfigPortalSSID());

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 80);
  tft.println("2. A setup page should open");
  tft.setCursor(10, 93);
  tft.println("   automatically. If not, browse to:");
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(20, 108);
  tft.println(WiFi.softAPIP().toString());

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 128);
  tft.println("3. Choose your WiFi network,");
  tft.setCursor(10, 141);
  tft.println("   enter the password, and Save.");
}

void connectWiFi() {
  Serial.println("=== WiFi connect (WiFiManager) ===");

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(180); // give up and restart after 3 min unconfigured
  wifiManager.setBreakAfterConfig(true);   // return after ONE attempt, success or fail,
                                            // instead of silently re-looping the portal
                                            // forever with no on-screen feedback

  // Tries saved credentials first; if that fails, starts a "CYD-Setup"
  // access point + captive portal for the user to configure WiFi.
  bool connected = wifiManager.autoConnect("CYD-Setup");

  if (connected) {
    Serial.println();
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Signal strength (RSSI): %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println();
    Serial.println("WiFi connection attempt failed (bad password, SSID out of range, or timed out).");
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("WiFi connection");
    tft.println("failed!");
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("");
    tft.println("Check the password was typed");
    tft.println("correctly. Restarting to try");
    tft.println("the setup portal again...");
    delay(4000);
    ESP.restart();
  }
}

#endif // WIFI_SETUP_H
