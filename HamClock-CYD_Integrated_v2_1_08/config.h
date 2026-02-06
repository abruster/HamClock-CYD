// ============================================================================
// config.h - Hardware constants + temporary dummy values
// ============================================================================
#ifndef CONFIG_H
#define CONFIG_H

// Display dimensions (keep - hardware constants)
#define MAP_WIDTH       240
#define MAP_HEIGHT      144
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   240

// Panel layout (keep)
#define INFO_PANEL_Y    MAP_HEIGHT
#define INFO_PANEL_HEIGHT (SCREEN_HEIGHT - MAP_HEIGHT)

// Update intervals (keep - timing constants)
#define WIFI_RETRY_MS        10000
#define NTP_RETRY_MS         60000
#define ISS_FETCH_INTERVAL   20000  // 20 seconds instead of 10
#define SOLAR_FETCH_INTERVAL 300000
#define WEATHER_FETCH_INTERVAL 600000
#define PSK_FETCH_INTERVAL   300000

// Display pin (keep)
#define BACKLIGHT_PIN   21

// ============================================================================
// TEMPORARY DUMMY VALUES - Not used, exist only for compilation
// Real values loaded from LittleFS/NVS at runtime
// ============================================================================
#define WIFI_SSID       "dummy"
#define WIFI_PASSWORD   "dummy"
#define MY_CALLSIGN     ""
#define MY_GRID         ""
#define WEATHER_API_KEY "dummy"
#define WEATHER_CITY    "dummy"
#define WEATHER_COUNTRY "dummy"
#define GMT_OFFSET_SEC      0
#define DAYLIGHT_OFFSET_SEC 0
#define LOCAL_OFFSET_SEC    0

// ============================================================================
// ADD THIS SECTION HERE - HamClockConfig structure
// ============================================================================
struct HamClockConfig {
  // API & Station Info
  char apiKey[64];
  bool apiKeyValid;
  char n2yoApiKey[64];
  bool n2yoApiKeyValid;
  float n2yoLatitude;
  float n2yoLongitude;
  char callsign[16];
  char gridSquare[8];
  
  // Location
  float latitude;
  float longitude;
  
  // Display settings
  uint16_t localTimeColour;
  uint16_t localFrameColour;
  uint16_t utcTimeColour;
  uint16_t utcFrameColour;
  uint16_t bannerColour;
  bool italicClockFonts;
  bool doubleFrame;
  int bannerSpeed;
  int bannerPixelsPerFrame;
  int screenSaverTimeout;
  bool autoPageChange;
  char startupLogo[32];
  unsigned long splashDuration;  // ms — minimum time splash stays on screen before first page draws
  
  // Time settings
  int utcOffsetSeconds;
};

// External declaration
extern HamClockConfig config;

#endif
