#ifndef SECRETS_H
#define SECRETS_H

// ---------- METAR STATION ----------
// One-time default used only on first boot. After that, whatever is
// saved via the on-board web form (http://<board-ip>/) takes over.
#define DEFAULT_METAR_STATION "VVDN"   // Da Nang Airport ICAO code

// User-Agent required by the aviationweather.gov API.
#define API_USER_AGENT "CYD-Weather-Display/1.0 (your_email@example.com)"

// ---------- Location (for the Open-Meteo rain history graph) ----------
// Open-Meteo needs coordinates, not an ICAO code - these default to
// Da Nang Airport (VVDN). Update if you move the METAR station.
#define WEATHER_LAT "16.0439"
#define WEATHER_LON "108.1994"

// ---------- EPHEMERIS (FreeAstroAPI) ----------
// https://www.freeastroapi.com - sign up for a key.
#define EPHEMERIS_API_KEY "Your_key"

// ---------- KITE WIND ALERT (email via Gmail SMTP) ----------
// Use a Gmail App Password, not your normal account password:
// https://myaccount.google.com/apppasswords
#define EMAIL_SENDER_ACCOUNT  "email_address"
#define EMAIL_SENDER_PASSWORD "your_password"
#define EMAIL_RECIPIENT       "email_address"


// Da Nang is UTC+7 -> offset in minutes, used to convert the board's
// UTC clock to local time for the alert window check below.
#define DANANG_UTC_OFFSET_MIN (7 * 60)

// Alert window in LOCAL (Da Nang) minutes-of-day.
// Example: 6:00 AM - 6:00 PM
#define KITE_WINDOW_START_MIN (6 * 60)
#define KITE_WINDOW_END_MIN   (18 * 60)

#endif // SECRETS_H
