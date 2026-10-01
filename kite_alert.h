#ifndef KITE_ALERT_H
#define KITE_ALERT_H

#include "globals.h"

/*
  kite_alert.h
  ------------
  Checks the current wind speed against KITE_MIN_WIND_KMH and emails
  EMAIL_RECIPIENT (via Gmail SMTP) the first time it's crossed each day.
  Uses a UTC-day-key reset trick (mirroring the Ephemeris call counter)
  so it only fires once per day even though weather refetches every
  30 min.
*/

// Callback ESP_Mail_Client uses to report SMTP progress/errors to Serial.
void smtpCallback(SMTP_Status status) {
  Serial.print("  [SMTP] ");
  Serial.println(status.info());
}

bool sendKiteAlertEmail() {
  Serial.println("=== sendKiteAlertEmail() ===");
  Serial.printf("  Sender:    %s\n", EMAIL_SENDER_ACCOUNT);
  Serial.printf("  Recipient: %s\n", EMAIL_RECIPIENT);
  Serial.println("  SMTP host: smtp.gmail.com:465 (SSL)");

  ESP_Mail_Session session;
  session.server.host_name   = "smtp.gmail.com";
  session.server.port        = 465; // SSL
  session.login.email        = EMAIL_SENDER_ACCOUNT;
  session.login.password     = EMAIL_SENDER_PASSWORD;
  session.login.user_domain  = "";

  SMTP_Message message;
  message.sender.name    = "CYD Weather Display";
  message.sender.email   = EMAIL_SENDER_ACCOUNT;
  message.subject        = "Kite alert: wind is up!";
  message.addRecipient("Kite Flyer", EMAIL_RECIPIENT);

  char body[220];
  snprintf(body, sizeof(body),
           "Wind speed is currently %.1f km/h (threshold: %.1f km/h) at %s.\n"
           "Data timestamp: %s\n"
           "Might be a good time to fly a kite!",
           currentWindSpeed, KITE_MIN_WIND_KMH, currentStation.c_str(),
           currentWeatherTime.c_str());
  message.text.content = body;
  Serial.println("  Message body:");
  Serial.println(body);

  smtp.callback(smtpCallback);

  Serial.println("  Connecting to SMTP server...");
  if (!smtp.connect(&session)) {
    Serial.printf("  SMTP connect FAILED: %s\n", smtp.errorReason().c_str());
    return false;
  }
  Serial.println("  SMTP connected OK. Sending message...");

  bool ok = MailClient.sendMail(&smtp, &message);
  if (ok) {
    Serial.println("  Message sent OK.");
  } else {
    Serial.printf("  Send FAILED: %s\n", smtp.errorReason().c_str());
  }

  Serial.println("  Closing SMTP session.");
  smtp.closeSession();
  return ok;
}

void checkKiteWindAlert() {
  Serial.println("=== checkKiteWindAlert() ===");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 500)) {
    Serial.println("  NTP time not available yet - skipping this check.");
    return;
  }

  int utcMinutesOfDay = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int localMinutesOfDay = (utcMinutesOfDay + DANANG_UTC_OFFSET_MIN) % 1440;
  bool inWindow = (localMinutesOfDay >= KITE_WINDOW_START_MIN &&
                    localMinutesOfDay <= KITE_WINDOW_END_MIN);

  Serial.printf("  UTC time: %02d:%02d  ->  Local time: %02d:%02d  (window: %02d:%02d-%02d:%02d, inWindow=%s)\n",
                timeinfo.tm_hour, timeinfo.tm_min,
                localMinutesOfDay / 60, localMinutesOfDay % 60,
                KITE_WINDOW_START_MIN / 60, KITE_WINDOW_START_MIN % 60,
                KITE_WINDOW_END_MIN / 60, KITE_WINDOW_END_MIN % 60,
                inWindow ? "yes" : "no");

  if (!inWindow) {
    Serial.println("  Outside the alert window - skipping.");
    return;
  }

  Serial.printf("  Wind speed: %.1f km/h  (threshold: %.1f km/h)\n",
                currentWindSpeed, KITE_MIN_WIND_KMH);

  if (currentWindSpeed < KITE_MIN_WIND_KMH) {
    Serial.println("  Below threshold - skipping.");
    return;
  }

  Serial.println("  Threshold met and in window - attempting to send email.");

  if (sendKiteAlertEmail()) {
    Serial.println("  >>> Kite alert email sent OK.");
  } else {
    Serial.println("  >>> Kite alert email FAILED to send.");
  }
}

#endif // KITE_ALERT_H
