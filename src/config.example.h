// Define if you want a instant update on startup
const bool DebugDisplayUpdate = false;

// Change to your WiFi credentials
const char* ssid     = "your_wifi_ssid";         // WiFi SSID to connect to
const char* password = "your_wifi_password";     // WiFi password needed for the SSID

// Use your own API key by signing up for a free developer account at https://openweathermap.org/
String apikey       = "your_owm_api_key";                   // See: https://openweathermap.org/  // It's free to get an API key, but don't take more than 60 readings/minute!
const char server[] = "api.openweathermap.org";
//http://api.openweathermap.org/data/2.5/weather?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=1   // Example API call for weather data
//http://api.openweathermap.org/data/2.5/forecast?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=40 // Example API call for forecast data
//Set your location according to OWM locations

String LAT              = "0.00";                          // Home location Latitude
String LON              = "0.00";                          // Home location Longitude

String City             = "YourCity";                      // Your home city See: http://bulk.openweathermap.org/sample/
String Country          = "DE";                            // Your _ISO-3166-1_two-letter_country_code country code, on OWM find your nearest city and the country code is displayed
                                                           // https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes
String Language         = "DE";                            // NOTE: Only the weather description is translated by OWM
                                                           // Examples: Arabic (AR) Czech (CZ) English (EN) Greek (EL) Persian(Farsi) (FA) Galician (GL) Hungarian (HU) Japanese (JA)
                                                           // Korean (KR) Latvian (LA) Lithuanian (LT) Macedonian (MK) Slovak (SK) Slovenian (SL) Vietnamese (VI)
String Hemisphere       = "north";                         // or "south"  
String Units            = "M";                             // Use 'M' for Metric or I for Imperial 
const char* Timezone    = "CET-1CEST,M3.5.0,M10.5.0/3";    // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
                                                           // See below for examples
const char* ntpServer   = "pool.ntp.org";                  // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
                                                           // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
                                                           // EU "0.europe.pool.ntp.org"
                                                           // US "0.north-america.pool.ntp.org"
                                                           // See: https://www.ntppool.org/en/                                                           
int   gmtOffset_sec     = 3600; // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int  daylightOffset_sec = 3600; // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset

// Example time zones
//const char* Timezone = "MET-1METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
//const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";       // Central Europe
//const char* Timezone = "EST-2METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
//const char* Timezone = "EST5EDT,M3.2.0,M11.1.0";           // EST USA  
//const char* Timezone = "CST6CDT,M3.2.0,M11.1.0";           // CST USA
//const char* Timezone = "MST7MDT,M4.1.0,M10.5.0";           // MST USA
//const char* Timezone = "NZST-12NZDT,M9.5.0,M4.1.0/3";      // Auckland
//const char* Timezone = "EET-2EEST,M3.5.5/0,M10.5.5/0";     // Asia
//const char* Timezone = "ACST-9:30ACDT,M10.1.0,M4.1.0/3":   // Australia

// Google Calendar integration (via Google Apps Script Web App bridge)
// See docs/superpowers/plans/2026-11-20-google-calendar.md for the deployment script and setup steps.
const char* calendarBridgeUrl   = ""; // Deployed Google Apps Script Web App URL, e.g. "https://script.google.com/macros/s/AKfy.../exec"
const char* calendarBridgeToken = ""; // Shared secret token validated by the Apps Script (?token=...) so the URL cannot be queried publicly
const char* calendarId1      = ""; // Google Calendar ID for cal1 (e.g. "primary", "cal1@gmail.com", or "...@group.calendar.google.com")
const char* calendarId2      = ""; // Google Calendar ID for cal2 (must be shared read-only with the Google account running the Apps Script)

// Fuel prices (Tankerkoenig API: https://creativecommons.tankerkoenig.de/)
const char* tankerkoenigApiKey  = "";
const char* fuelStationIds[3]   = { "", "", "" }; // Up to 3 station UUIDs
const char* fuelStationNames[3] = { "", "", "" }; // Short display labels (e.g. "Aral", "Shell", "Jet")

// Travel times (Mapbox Directions API: https://docs.mapbox.com/api/navigation/directions/)
const char* mapboxAccessToken   = "";
const char* travelDestNames[2]  = { "", "" }; // Short destination labels (e.g. "Arbeit", "Zentrum")
const char* travelDestLats[2]   = { "", "" }; // Destination latitudes
const char* travelDestLons[2]   = { "", "" }; // Destination longitudes

