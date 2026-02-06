// ============================================================================
// FILE: display_manager.h
// ============================================================================
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include "psk_reporter.h"
#include "solar_data.h"
#include "weather_data.h"

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define MAP_WIDTH 240
#define MAP_HEIGHT 144
#define INFO_PANEL_Y 144
#define INFO_PANEL_HEIGHT 96
#define BACKLIGHT_PIN 27
//////////////////////////
//added 1/22/26
struct DisplayConfig {
  bool doubleFrame;
  bool italicClockFonts;
  uint16_t localFrameColour;
  uint16_t utcFrameColour;
  uint16_t localTimeColour;
  uint16_t utcTimeColour;
  int bannerSpeed;
  int bannerPixelsPerFrame;  // ADD THIS
  uint16_t bannerColour;
};
///////////////////////////

// Page definitions
enum DisplayPage {
  PAGE_MAIN = 0,      // Map with band conditions
  PAGE_ISS,           // ISS position data
  PAGE_PROPAGATION,   // Band conditions day/night + basic solar
  PAGE_SOLAR,         // Detailed solar data
  PAGE_PSK,           // PSK Reporter
  PAGE_WEATHER,       // Weather details
  PAGE_STATS,         // System stats (always last)
  PAGE_COUNT          // Total number of pages
};

// Band condition levels
enum BandCondition {
  BAND_EXCELLENT,
  BAND_GOOD,
  BAND_FAIR,
  BAND_POOR
};

// Forward declarations
class ISSTracker;

class DisplayManager {
public:
  DisplayManager(TFT_eSPI* tft);

  void setConfig(const DisplayConfig& config);  // Add this method
  void begin();
  
  // Returns the web-safe version of the current scrolling banner text
  // (uses &deg; instead of 0xB0 for HTML display)
  String getWebBannerText();
  
  // Page management
  void nextPage();
  void prevPage();
  DisplayPage getCurrentPage() { return _currentPage; }
  void setPage(DisplayPage page);
  
  // Main draw function - called every loop
void drawCurrentPage(bool wifiConnected, bool forceUpdate, 
                    SolarData* solarData, WeatherData* weatherData, 
                    PSKReporter* pskReporter,
                    float issLat, float issLon,
                    ISSTracker* issTracker,
                    bool timeValid);  // ADD THIS PARAMETER
  
  // Touch handling
  bool handleTouch(uint16_t x, uint16_t y);
  
  // Map and grayline (used by main page)
  void drawMap();
  bool drawGrayline();
  void forceMapRedraw();
  void drawISS(float lat, float lon, bool clearPrevious = true);
  
private:
  TFT_eSPI* _tft;
  DisplayConfig _config;  // Add this line
  DisplayPage _currentPage;
  static String _webBannerText;  // Web-safe banner text (persists between frames)
  int _lastISSX;
  int _lastISSY;
  unsigned long _bootTime;
  
// Individual page draw functions
  void drawMainPage(bool wifiConnected, bool timeValid, const char* timeStr, SolarIndices solarData, float issLat, float issLon, int timezoneOffsetSeconds, WeatherInfo weather);
  void drawISSPage(float issLat, float issLon, bool timeValid, const char* timeStr, ISSTracker* issTracker);
  void drawPropagationPage(SolarIndices solarData, bool timeValid, const char* timeStr);
  void drawSolarPage(SolarIndices solarData, bool timeValid, const char* timeStr);
  void drawPSKPage(PSKReporter* pskReporter, bool timeValid, const char* timeStr);
  void drawWeatherPage(WeatherInfo weather, bool timeValid, const char* timeStr);
  void drawStatsPage(bool wifiConnected, bool timeValid, const char* timeStr,
                     SolarIndices solarData, WeatherInfo weather, 
                     PSKReporter* pskReporter, float issLat, float issLon);
  
  // Helper functions
  void drawPanel(int x, int y, int w, int h, const char* title);
  BandCondition calculateBandCondition(String band, SolarIndices indices, bool isDay);
  uint16_t conditionToColor(BandCondition condition);
  void drawBandConditions(SolarIndices indices, bool timeValid);
  
  // Legacy function (kept for compatibility)
  void drawInfoPanel(bool wifiConnected, bool timeValid, SolarIndices indices, WeatherInfo weather);
};

#endif
