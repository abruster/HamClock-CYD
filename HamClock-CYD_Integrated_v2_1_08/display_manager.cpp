// ============================================================================
// FILE: display_manager.cpp - Multi-Page System
// ============================================================================
#include <Arduino.h>
#include "display_manager.h"

// Static member initialization
String DisplayManager::_webBannerText = "";

// Getter for web-safe banner text (called by /scrolltext handler)
String DisplayManager::getWebBannerText() {
  return _webBannerText;
}
#include "iss_tracker.h"
#include "config.h"
#include "world_map.h"
#include "grayline.h"
#include <WiFi.h>
#include "fonts/DINPro_Regular6pt8b.h"  //Extended character set includes degree symbol ° and stroke Ø
#include "fonts/DINPro_Regular8pt7b.h"
#include "fonts/DINPro_Bold8pt7b.h"
#include "fonts/DINPro_Regular11pt7b.h"
#include "fonts/DINPro_Regular15pt8b.h" //Extended character set includes degree symbol ° and stroke Ø
#include "fonts/DINPro_Regular18pt7b.h"
#include "fonts/digital19pt7b.h"
#include "fonts/digitalitalic19pt7b.h"
#include "fonts/DINPro_Light15pt7b.h"

DisplayManager::DisplayManager(TFT_eSPI* tft) 
  : _tft(tft), _currentPage(PAGE_MAIN), _lastISSX(-1), _lastISSY(-1), _bootTime(0) {}

int timezoneOffsetSeconds = 0;  // Timezone offset from UTC in seconds

// Config setter - RIGHT HERE
void DisplayManager::setConfig(const DisplayConfig& config) {
  _config = config;
  Serial.printf("✅ DisplayManager::setConfig() called - bannerSpeed now %d\n", _config.bannerSpeed);
}

void DisplayManager::begin() {
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  
  _tft->init();
  _tft->setRotation(1);
  _tft->fillScreen(TFT_BLACK);
  
  _bootTime = millis();
  _currentPage = PAGE_MAIN;
}

// ============================================================================
// PAGE MANAGEMENT
// ============================================================================

void DisplayManager::nextPage() {
  int nextIdx = (int)_currentPage + 1;
  if (nextIdx >= PAGE_COUNT) {
    nextIdx = 0;
  }
  _currentPage = (DisplayPage)nextIdx;
  _tft->fillScreen(TFT_BLACK);
}

void DisplayManager::prevPage() {
  int prevIdx = (int)_currentPage - 1;
  if (prevIdx < 0) {
    prevIdx = PAGE_COUNT - 1;
  }
  _currentPage = (DisplayPage)prevIdx;
  _tft->fillScreen(TFT_BLACK);
}

void DisplayManager::setPage(DisplayPage page) {
  if (page >= 0 && page < PAGE_COUNT && page != _currentPage) {
    _currentPage = page;
    _tft->fillScreen(TFT_BLACK);
  }
}

// ============================================================================
// TOUCH HANDLING
// ============================================================================

bool DisplayManager::drawGrayline() {
  static Grayline grayline;
  
  time_t now;
  time(&now);
  
  if (!grayline.needsRedraw(now)) {
    return false;
  }
  
  grayline.update(now);
  
  drawMap();
  
  for(int x = 0; x < MAP_WIDTH; x++) {
    for(int y = 0; y < MAP_HEIGHT; y++) {
      float lon = map(x, 0, MAP_WIDTH - 1, -180.0 * 100, 180.0 * 100) / 100.0;
      float lat = map(y, 0, MAP_HEIGHT - 1, 90.0 * 100, -90.0 * 100) / 100.0;
      
      if (!grayline.isDaylight(lat, lon)) {
        uint16_t pixelColor = _tft->readPixel(x, y);
        
        uint8_t r = (pixelColor >> 11) & 0x1F;
        uint8_t g = (pixelColor >> 5) & 0x3F;
        uint8_t b = pixelColor & 0x1F;
        
        r = (r * 40) / 100;
        g = (g * 40) / 100;
        b = (b * 40) / 100;
        
        uint16_t darkColor = (r << 11) | (g << 5) | b;
        _tft->drawPixel(x, y, darkColor);
      }
    }
  }
  
  return true;
}

void DisplayManager::forceMapRedraw() {
  drawMap();
  
  static Grayline grayline;
  time_t now;
  time(&now);
  
  grayline.update(now);
  
  for(int x = 0; x < MAP_WIDTH; x++) {
    for(int y = 0; y < MAP_HEIGHT; y++) {
      float lon = map(x, 0, MAP_WIDTH - 1, -180.0 * 100, 180.0 * 100) / 100.0;
      float lat = map(y, 0, MAP_HEIGHT - 1, 90.0 * 100, -90.0 * 100) / 100.0;
      
      if (!grayline.isDaylight(lat, lon)) {
        uint16_t pixelColor = _tft->readPixel(x, y);
        
        uint8_t r = (pixelColor >> 11) & 0x1F;
        uint8_t g = (pixelColor >> 5) & 0x3F;
        uint8_t b = pixelColor & 0x1F;
        
        r = (r * 40) / 100;
        g = (g * 40) / 100;
        b = (b * 40) / 100;
        
        uint16_t darkColor = (r << 11) | (g << 5) | b;
        _tft->drawPixel(x, y, darkColor);
      }
    }
  }
}

void DisplayManager::drawISS(float lat, float lon, bool clearPrevious) {
  int x = map(lon, -180, 180, 0, MAP_WIDTH-1);
  int y = map(lat, 90, -90, 0, MAP_HEIGHT-1);
  
  if (clearPrevious && _lastISSX >= 0 && _lastISSY >= 0) {
    _tft->fillCircle(_lastISSX, _lastISSY, 3, TFT_BLACK);
  }
  
  _tft->fillCircle(x, y, 3, TFT_RED);
  
  _lastISSX = x;
  _lastISSY = y;
}

BandCondition DisplayManager::calculateBandCondition(String band, SolarIndices indices, bool isDay) {
  bool highSFI = indices.sfi > 120;
  bool medSFI = indices.sfi > 90;
  bool lowK = indices.kIndex < 3;
  bool veryLowK = indices.kIndex < 2;
  
  if (band == "160m") {
    if (!isDay && lowK) return BAND_GOOD;
    if (!isDay) return BAND_FAIR;
    return BAND_POOR;
  }
  
  if (band == "80m") {
    if (!isDay && lowK) return BAND_EXCELLENT;
    if (!isDay) return BAND_GOOD;
    if (lowK) return BAND_FAIR;
    return BAND_POOR;
  }
  
  if (band == "40m") {
    if (veryLowK) return BAND_EXCELLENT;
    if (lowK) return BAND_GOOD;
    return BAND_FAIR;
  }
  
  if (band == "30m") {
    if (veryLowK && medSFI) return BAND_EXCELLENT;
    if (lowK) return BAND_GOOD;
    return BAND_FAIR;
  }
  
  if (band == "20m") {
    if (isDay && highSFI && veryLowK) return BAND_EXCELLENT;
    if (isDay && medSFI && lowK) return BAND_GOOD;
    if (isDay || (medSFI && lowK)) return BAND_FAIR;
    return BAND_POOR;
  }
  
  if (band == "17m") {
    if (isDay && highSFI && lowK) return BAND_GOOD;
    if (isDay && medSFI) return BAND_FAIR;
    return BAND_POOR;
  }
  
  if (band == "15m") {
    if (isDay && highSFI && lowK) return BAND_GOOD;
    if (isDay && highSFI) return BAND_FAIR;
    return BAND_POOR;
  }
  
  if (band == "12m") {
    if (isDay && highSFI && lowK) return BAND_FAIR;
    if (isDay && highSFI) return BAND_POOR;
    return BAND_POOR;
  }
  
  if (band == "10m") {
    if (isDay && indices.sfi > 150 && lowK) return BAND_GOOD;
    if (isDay && highSFI) return BAND_FAIR;
    return BAND_POOR;
  }
  
  if (band == "6m") {
    if (isDay && highSFI) return BAND_FAIR;
    return BAND_POOR;
  }
  
  return BAND_POOR;
}

uint16_t DisplayManager::conditionToColor(BandCondition condition) {
  switch (condition) {
    case BAND_EXCELLENT: return TFT_GREEN;
    case BAND_GOOD:      return 0x07E0;
    case BAND_FAIR:      return TFT_YELLOW;
    case BAND_POOR:      return TFT_RED;
    default:             return TFT_DARKGREY;
  }
}

void DisplayManager::drawBandConditions(SolarIndices indices, bool timeValid) {
  int panel_x = MAP_WIDTH;
  int panel_y = 0;
  int panel_w = SCREEN_WIDTH - MAP_WIDTH;
  int panel_h = MAP_HEIGHT;
  
  _tft->drawRect(panel_x, panel_y, panel_w, panel_h, TFT_WHITE);
  
  _tft->setTextSize(1);
  _tft->setTextColor(TFT_WHITE, TFT_BLACK);
  _tft->setCursor(panel_x + 2, panel_y + 2);
  _tft->print("Bands");
  
  bool isDay = true;
  if (timeValid) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      isDay = (timeinfo.tm_hour >= 6 && timeinfo.tm_hour < 18);
    }
  }
  
  _tft->setTextColor(isDay ? TFT_YELLOW : TFT_CYAN, TFT_BLACK);
  _tft->setCursor(panel_x + 38, panel_y + 2);
  _tft->print(isDay ? "DAY  " : "NIGHT");
  
  String bands[] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"};
  
  int y_start = panel_y + 15;
  int box_height = 12;
  int spacing = 1;
  
  for (int i = 0; i < 10; i++) {
    int box_y = y_start + (i * (box_height + spacing));
    
    BandCondition condition = calculateBandCondition(bands[i], indices, isDay);
    uint16_t color = conditionToColor(condition);
    
    _tft->fillRect(panel_x + 2, box_y, 30, box_height, color);
    
    _tft->setTextSize(1);
    _tft->setTextColor(TFT_BLACK, color);
    _tft->setCursor(panel_x + 4, box_y + 2);
    _tft->print(bands[i]);
    
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(panel_x + 35, box_y + 2);
    
    switch (condition) {
      case BAND_EXCELLENT: _tft->print("EXC"); break;
      case BAND_GOOD:      _tft->print("GD "); break;
      case BAND_FAIR:      _tft->print("FR "); break;
      case BAND_POOR:      _tft->print("PR "); break;
    }
  }
}

void DisplayManager::drawPanel(int x, int y, int w, int h, const char* title) {
  _tft->drawRect(x, y, w, h, TFT_WHITE);
  
  if (title != nullptr && strlen(title) > 0) {
    _tft->fillRect(x + 1, y + 1, w - 2, 10, TFT_DARKGREY);
    _tft->setTextSize(1);
    _tft->setTextColor(TFT_WHITE);
    _tft->setCursor(x + 3, y + 2);
    _tft->print(title);
  }
}

// Legacy function - kept for backward compatibility
void DisplayManager::drawInfoPanel(bool wifiConnected, bool timeValid, 
                                   SolarIndices indices, WeatherInfo weather) {
  // This function is no longer used in the multi-page system
  // but kept for compatibility if needed
}

bool DisplayManager::handleTouch(uint16_t x, uint16_t y) {
  // Left third of screen = previous page
  // Right third of screen = next page
  // Middle third = no action (or could be used for page-specific actions)
  
  if (x < SCREEN_WIDTH / 3) {
    prevPage();
    return true;
  } else if (x > (SCREEN_WIDTH * 2 / 3)) {
    nextPage();
    return true;
  }
  
  return false;
}

// ============================================================================
// MAIN DRAW FUNCTION
// ============================================================================

void DisplayManager::drawCurrentPage(bool wifiConnected, bool forceUpdate,
                                     SolarData* solarData, WeatherData* weatherData,
                                     PSKReporter* pskReporter,
                                     float issLat, float issLon,
                                     ISSTracker* issTracker,
                                     bool timeValid) {  // ADD THIS
          
  static DisplayPage lastPage = PAGE_COUNT; // Invalid initial value
  static unsigned long lastUpdate = 0;
  static unsigned long lastISSUpdate = 0;      // Separate timer for ISS page
  static unsigned long lastStatsUpdate = 0;    // Separate timer for Stats page
  
  // Track last update strings to detect data changes
  static String lastSolarUpdate = "";
  static String lastWeatherUpdate = "";
  static String lastPskUpdate = "";

  // Detect page change
  bool pageChanged = (_currentPage != lastPage);
  if (pageChanged) {
    lastPage = _currentPage;
  }
  
  unsigned long now = millis();

  // Get timezone offset (use weather's timezone or default to 0)
  int timezoneOffset = (weatherData && weatherData->isDataValid()) ? 
                       weatherData->getWeather().timezoneOffsetSeconds : 0;

  // Get static update strings for data pages (local time, no seconds)
  String solarUpdateStr = solarData ? solarData->getLastUpdateString(timezoneOffset) : "Updated: --:--";
  String weatherUpdateStr = weatherData ? weatherData->getLastUpdateString(timezoneOffset) : "Updated: --:--";
  String pskUpdateStr = pskReporter ? pskReporter->getLastUpdateString(timezoneOffset) : "Updated: --:--";
  
  // Detect if data has actually changed
  bool solarDataChanged = (solarUpdateStr != lastSolarUpdate);
  bool weatherDataChanged = (weatherUpdateStr != lastWeatherUpdate);
  bool pskDataChanged = (pskUpdateStr != lastPskUpdate);
  
  // Determine if we should redraw based on current page
  bool shouldRedraw = pageChanged;
  
  switch (_currentPage) {
    case PAGE_MAIN:
      // Main page: update frequently for smooth scrolling banner (controlled by main loop timing)
      shouldRedraw = true;  // Always allow main page to update (for banner scrolling)
      break;
      
    case PAGE_ISS:
      // ISS page: limit to once per second maximum
      if (now - lastISSUpdate >= 1000) {
        shouldRedraw = true;
        lastISSUpdate = now;
      }
      break;
      
    case PAGE_PROPAGATION:
    case PAGE_SOLAR:
      // Solar pages: only update when solar data changes
      shouldRedraw = shouldRedraw || solarDataChanged;
      break;
      
    case PAGE_PSK:
      // PSK page: only update when PSK data changes
      shouldRedraw = shouldRedraw || pskDataChanged;
      break;
      
    case PAGE_WEATHER:
      // Weather page: only update when weather data changes
      shouldRedraw = shouldRedraw || weatherDataChanged;
      break;
      
    case PAGE_STATS:
      // Stats page: limit to once per second maximum
      if (now - lastStatsUpdate >= 1000) {
        shouldRedraw = true;
        lastStatsUpdate = now;
      }
      break;
  }
  
  // If no redraw needed, return early
  if (!shouldRedraw) {
    return;
  }
  
  // Update last update time and strings
  lastUpdate = now;
  lastSolarUpdate = solarUpdateStr;
  lastWeatherUpdate = weatherUpdateStr;
  lastPskUpdate = pskUpdateStr;

  // Get time once at the start - pass to all pages
  struct tm timeinfo;
  bool timeAvailable = timeValid && getLocalTime(&timeinfo);
  char timeStrShort[16] = "";       // "HH:MM UTC" for main page
  char timeStrStats[32] = "";       // "YYYY-MM-DD HH:MM:SS UTC" for stats page
  char timeStrISS[32] = "";         // "Updated: HH:MM:SS UTC" for ISS page
  
  if (timeAvailable) {
    strftime(timeStrShort, sizeof(timeStrShort), "%H:%M UTC", &timeinfo);
    strftime(timeStrStats, sizeof(timeStrStats), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
    strftime(timeStrISS, sizeof(timeStrISS), "Updated: %H:%M:%S UTC", &timeinfo);
  }
  
  // Get data structures  
  SolarIndices solarIndices = solarData ? solarData->getIndices() : SolarIndices{0,0,0,0,"",0.0};
  WeatherInfo weather = weatherData ? weatherData->getWeather() : WeatherInfo{};
    
  switch (_currentPage) {
    case PAGE_MAIN:
      drawMainPage(wifiConnected, timeAvailable, timeStrShort, solarIndices, issLat, issLon, 
                   timezoneOffset, weather);
      break;
    case PAGE_ISS:
      drawISSPage(issLat, issLon, timeAvailable, timeStrISS, issTracker);
      break;
    case PAGE_PROPAGATION:
      drawPropagationPage(solarIndices, timeAvailable, solarUpdateStr.c_str());
      break;
    case PAGE_SOLAR:
      drawSolarPage(solarIndices, timeAvailable, solarUpdateStr.c_str());
      break;
    case PAGE_PSK:
      drawPSKPage(pskReporter, timeAvailable, pskUpdateStr.c_str());
      break;
    case PAGE_WEATHER:
      drawWeatherPage(weather, timeAvailable, weatherUpdateStr.c_str());
      break;
    case PAGE_STATS:
      drawStatsPage(wifiConnected, timeAvailable, timeStrStats, solarIndices, weather, pskReporter, issLat, issLon);
      break;
  }
}

// ============================================================================
// PAGE 1: MAIN PAGE (Map + Band Conditions)
// ============================================================================

void DisplayManager::drawMainPage(bool wifiConnected, bool timeValid, const char* timeStr,
                                  SolarIndices solarData, float issLat, float issLon,
                                  int timezoneOffsetSeconds, WeatherInfo weather) {  // ADD THIS PARAMETER
  static bool pageInitialized = false;
  static unsigned long lastGraylineUpdate = 0;
  static unsigned long lastISSUpdate = 0;
  static unsigned long lastClockUpdate = 0;
  static int scrollPosition = SCREEN_WIDTH;
  static unsigned long lastScrollUpdate = 0;
  static String lastUTCTime = "";
  static String lastLocalTime = "";
  
  // First time on this page - draw everything
  if (!pageInitialized) {
    drawMap();
    forceMapRedraw();
    pageInitialized = true;
    lastGraylineUpdate = millis();
    lastISSUpdate = millis();
    scrollPosition = SCREEN_WIDTH;
  }
  
  // Update grayline every 5 minutes
  if (millis() - lastGraylineUpdate > 300000) {
    if (drawGrayline()) {
      lastGraylineUpdate = millis();
    }
  }
  
  // Update ISS position every 30 seconds, but only if it moved significantly
  if (millis() - lastISSUpdate > 30000) {
    int x = map(issLon, -180, 180, 0, MAP_WIDTH-1);
    int y = map(issLat, 90, -90, 0, MAP_HEIGHT-1);
    
    // Only redraw if moved more than 5 pixels (reduces slowdown from frequent redraws)
    if (abs(x - _lastISSX) > 5 || abs(y - _lastISSY) > 5) {
      drawISS(issLat, issLon, true);
    }
    lastISSUpdate = millis();
  }
  
  // Draw band conditions panel with WiFi moved below PSK
  _tft->setFreeFont(nullptr);
  drawBandConditions(solarData, timeValid);
  
  // Add WiFi status to band panel (below existing bands)
  int panel_x = MAP_WIDTH;
  _tft->setTextSize(1);
  if (wifiConnected) {
    _tft->setTextColor(TFT_GREEN, TFT_BLACK);
    _tft->setCursor(panel_x + 55, 135);
    _tft->print("WiFi");
  } else {
    _tft->setTextColor(TFT_RED, TFT_BLACK);
    _tft->setCursor(panel_x + 55, 135);
    _tft->print("WiFi");
  }
  
// ============================================================================
// DIGITAL CLOCKS - Update every second
// ============================================================================
if (timeValid && (millis() - lastClockUpdate >= 100)) {  // 100ms gate — inner string compare prevents unnecessary redraws
  lastClockUpdate = millis();
  
//  Serial.printf("DEBUG: timezoneOffsetSeconds = %d (%d hours)\n", 
//                timezoneOffsetSeconds, timezoneOffsetSeconds/3600);
  
  // Get current time as Unix timestamp
  time_t now;
  time(&now);
  
  // Format UTC time
  struct tm* utcTimeInfo = gmtime(&now);
  char utcTime[9];
  strftime(utcTime, sizeof(utcTime), "%H:%M:%S", utcTimeInfo);
  
  // Format local time (add timezone offset)
  time_t localNow = now + timezoneOffsetSeconds;
  struct tm* localTimeInfo = gmtime(&localNow);
  char localTime[9];
  strftime(localTime, sizeof(localTime), "%H:%M:%S", localTimeInfo);
  
  String utcStr = String(utcTime);
  String localStr = String(localTime);
  
//  Serial.printf("DEBUG: UTC=%s, Local=%s\n", utcStr.c_str(), localStr.c_str());

  // Only redraw if time changed
  if (utcStr != lastUTCTime || localStr != lastLocalTime) {
  
// ===== LOCAL CLOCK =====
    // Clear the border area first

    _tft->drawSmoothRoundRect(1, 147, 9, 2, 156, 51,
                              TFT_BLACK, TFT_BLACK);
                          
    // Inverted logic: !doubleFrame = thin, doubleFrame = thick                       
    if (!_config.doubleFrame) {  // ← Changed to !

    // Thin: single 3-pixel frame
    _tft->drawSmoothRoundRect(4, 151, 8, 7, 150, 43, 
                              _config.localFrameColour, TFT_BLACK);
} else {
    // Thick: double frame (6 pixels total)                           
    _tft->drawSmoothRoundRect(1, 148, 10, 6, 156, 49,
                            _config.localFrameColour, TFT_BLACK);            
    _tft->drawSmoothRoundRect(4, 151, 8, 7, 150, 43, 
                            _config.localFrameColour, TFT_BLACK);
}
    // Label
    _tft->fillRect(60, 190, 40, 13, TFT_BLACK);
    _tft->setFreeFont(&DINPro_Bold8pt7b);
    _tft->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    _tft->setCursor(61, 201);
    _tft->print("Local");
    
    // Clear clock area
    _tft->fillRoundRect(8, 157, 143, 30, 10, TFT_BLACK);
    
    // Draw time (use italic font based on config)
    if (_config.italicClockFonts) {
      _tft->setFreeFont(&digitalitalic19pt7b);
    } else {
      _tft->setFreeFont(&digital19pt7b);
    }
    _tft->setTextColor(_config.localTimeColour, TFT_BLACK);
    _tft->setCursor(12, 183);
    _tft->print(localStr);

// ===== UTC CLOCK =====

    // Clear the border area first

    _tft->drawSmoothRoundRect(164, 147, 9, 2, 156, 51,
                              TFT_BLACK, TFT_BLACK);

// Inverted logic: !doubleFrame = thin, doubleFrame = thick                       
    if (!_config.doubleFrame) {  // ← Changed to !

    // Thin: single 3-pixel frame
    _tft->drawSmoothRoundRect(167, 151, 8, 7, 150, 43, 
                              _config.utcFrameColour, TFT_BLACK);
} else {
    // Thick: double frame (6 pixels total)                           
    _tft->drawSmoothRoundRect(164, 148, 10, 6, 156, 49,
                            _config.utcFrameColour, TFT_BLACK);            
    _tft->drawSmoothRoundRect(167, 151, 8, 7, 150, 43, 
                            _config.utcFrameColour, TFT_BLACK);
}
  
    // Label
    _tft->fillRect(226, 190, 32, 13, TFT_BLACK);
    _tft->setFreeFont(&DINPro_Bold8pt7b);
    _tft->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    _tft->setCursor(227, 201);
    _tft->print("UTC");
    
    // Clear clock area
    _tft->fillRoundRect(171, 157, 143, 30, 10, TFT_BLACK);
    
    // Draw time
    if (_config.italicClockFonts) {
      _tft->setFreeFont(&digitalitalic19pt7b);
    } else {
      _tft->setFreeFont(&digital19pt7b);
    }
    _tft->setTextColor(_config.utcTimeColour, TFT_BLACK);
    _tft->setCursor(174, 183);
    _tft->print(utcStr);
    
    lastUTCTime = utcStr;
    lastLocalTime = localStr;
  }
}
  
// ============================================================================
// SCROLLING WEATHER BANNER
// ============================================================================
if (millis() - lastScrollUpdate >= _config.bannerSpeed) {
  lastScrollUpdate = millis();
  
  // Declare strings at the top of the block
  String weatherText;
  String weatherTextWeb;
  
  // Build weather text from real data
  if (weather.valid && weather.tempF > -50) {  // ← ADD THIS CHECK
    // Build weather text for TFT display (with \xB0)
    //TFT version
    weatherText = "Current conditions near " + weather.cityName + " " + weather.country + "    ";
    weatherText += "Temperature: " + String((int)weather.tempF) + "\xB0" "F (feels like " + String((int)weather.feelsLikeF) + "\xB0" "F)    ";
    weatherText += "Humidity: " + String(weather.humidity) + "%    ";
    weatherText += "Pressure: " + String(weather.pressure, 1) + " inHg    ";
    weatherText += "Winds: " + weather.windDir + " @ " + String((int)weather.windSpeed) + "mph";
    if (weather.windGust > 0) {
      weatherText += " / gusts to " + String((int)weather.windGust) + "mph";
    }
    weatherText += "    " + weather.description;
    
    // Build weather text for WEB (with &deg;)
    //WEB version
    weatherTextWeb = "Current conditions near " + weather.cityName + " " + weather.country + "    ";
    weatherTextWeb += "Temperature: " + String((int)weather.tempF) + "&deg;F (feels like " + String((int)weather.feelsLikeF) + "&deg;F)    ";
    weatherTextWeb += "Humidity: " + String(weather.humidity) + "%    ";
    weatherTextWeb += "Pressure: " + String(weather.pressure, 1) + " inHg    ";
    weatherTextWeb += "Winds: " + weather.windDir + " @ " + String((int)weather.windSpeed) + "mph";
    if (weather.windGust > 0) {
      weatherTextWeb += " / gusts to " + String((int)weather.windGust) + "mph";
    }
    weatherTextWeb += "    " + weather.description;  
      
  } else {
    weatherText = "Weather data loading...";
    weatherTextWeb = "Weather data loading...";
  }
  
  // Store web version for /scrolltext endpoint
  _webBannerText = weatherTextWeb;
  
  // Create sprite for smooth rendering (only once)
  static TFT_eSprite banner = TFT_eSprite(_tft);
  static bool spriteCreated = false;
  if (!spriteCreated) {
    banner.createSprite(SCREEN_WIDTH, 31);
    spriteCreated = true;
  }
  
// Draw to sprite (off-screen)
banner.fillSprite(TFT_BLACK);
banner.setFreeFont(&DINPro_Regular15pt8b);
banner.setTextColor(_config.bannerColour, TFT_BLACK);
banner.setCursor(scrollPosition, 23);
banner.print(weatherText);

// Push sprite to screen
banner.pushSprite(0, 208);

// Update scroll position — always 1 pixel for smoothness
scrollPosition -= 1;

// Calculate ACTUAL text width using the font
int textWidth = banner.textWidth(weatherText);  // ← KEEP THIS ONE ONLY

// Reset when scrolls off
if (scrollPosition < -(textWidth + 50)) {
  scrollPosition = SCREEN_WIDTH;
}

}
_tft->setFreeFont(nullptr);
}




// ============================================================================
// PAGE 2: ISS POSITION
// ============================================================================
// ============================================================================
// Enhanced drawISSPage() function
// Add this to display_manager.cpp (replace existing drawISSPage)
// ============================================================================

void DisplayManager::drawISSPage(float issLat, float issLon, bool timeValid, const char* timeStr, ISSTracker* issTracker) {
  static bool pageInitialized = false;
  
  if (!pageInitialized) {
    _tft->fillScreen(TFT_BLACK);
    pageInitialized = true;
  }
  
  // Title
  _tft->setFreeFont(&DINPro_Regular15pt8b);
  _tft->setTextColor(TFT_CYAN, TFT_BLACK);
  _tft->setCursor(10, 20);
  _tft->print("ISS POSITION");
  
  // Current Location Section
  _tft->setFreeFont(&DINPro_Regular6pt8b);
  _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  _tft->setCursor(20, 50);
  _tft->print("Current Location:");
  
  // Clear the position area before redrawing
  _tft->fillRect(20, 64, 180, 32, TFT_BLACK);
  
  // Draw current position
  _tft->setTextColor(TFT_WHITE, TFT_BLACK);
  _tft->setCursor(40, 75);
  _tft->printf("Lat:  %9.4f", issLat);
  _tft->setCursor(40, 90);
  _tft->printf("Lon:  %9.4f", issLon);
  
  // Next Visible Pass Section
  _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  _tft->setCursor(20, 120);
  _tft->print("Next Visible Pass:");
  
  // Get pass data from ISS tracker
  // (You'll need to pass the issTracker object to this function)
  if (issTracker && issTracker->hasPassData()) {
    ISSPass nextPass = issTracker->getNextPass();
    
    // Clear the pass info area
    _tft->fillRect(20, 134, 280, 75, TFT_BLACK);
    
    // Convert UTC to local time (automatically handles DST)
    time_t riseTime = nextPass.riseTime;
    struct tm* timeInfo = localtime(&riseTime);
    char dateStr[32];
    char timeStrBuf[32];
    strftime(dateStr, sizeof(dateStr), "%b %d", timeInfo);
    strftime(timeStrBuf, sizeof(timeStrBuf), "%I:%M %p", timeInfo);  // 12-hour with AM/PM
    //strftime(timeStrBuf, sizeof(timeStrBuf), "%H:%M", timeInfo);  // 24-hour time    
    
    // Calculate duration
    int duration = (nextPass.setTime - nextPass.riseTime) / 60;
    
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Line 1: Date and Rise Time
    _tft->setCursor(40, 145);
    _tft->printf("%s at %s", dateStr, timeStrBuf);
    
    // Line 2: Duration and Direction
    _tft->setCursor(40, 160);
    if (strlen(nextPass.direction) > 0 && strcmp(nextPass.direction, "N/A") != 0) {
      _tft->printf("Duration: %d min  Dir: %s", duration, nextPass.direction);
    } else {
      _tft->printf("Duration: %d minutes", duration);
    }
    
    // Line 3: Max Elevation (if available)
    if (nextPass.maxElevation > 0) {
      _tft->setCursor(40, 175);
      _tft->printf("Max Elevation: %.0f", nextPass.maxElevation);
      _tft->print("\xB0");  // Degree symbol
    }
    
    // Line 4: Brightness (if available)
    if (nextPass.magnitude != 0) {
      _tft->setCursor(40, 190);
      _tft->printf("Brightness: %.1f mag", nextPass.magnitude);
      if (nextPass.magnitude < 0) {
        _tft->print(" (Bright!)");
      }
    }
    
  } else {
    // No pass data available
    _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    _tft->setCursor(40, 145);
    _tft->print("Fetching pass data...");
    _tft->setCursor(40, 160);
    _tft->print("Please wait...");
  }
  
  // Timestamp
  if (timeValid) {
    _tft->fillRect(10, 210, 130, 15, TFT_BLACK);
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(10, 225);
    _tft->print(timeStr);
  }
}

// ============================================================================
// PAGE 3: PROPAGATION (Band Conditions Day/Night)
// ============================================================================

void DisplayManager::drawPropagationPage(SolarIndices solarData, bool timeValid, const char* timeStr) {
  static bool pageInitialized = false;
//  static unsigned long lastFullUpdate = 0;
  
  if (!pageInitialized) {
    _tft->fillScreen(TFT_BLACK);
    pageInitialized = true;
  }
  
  // Throttle updates
//  if (millis() - lastFullUpdate < 1000 && pageInitialized) {
//    return;
//  }
//  lastFullUpdate = millis();
  
  // Title
  _tft->setFreeFont(&DINPro_Regular15pt8b);
  _tft->setTextColor(TFT_CYAN, TFT_BLACK);
  _tft->setCursor(10, 20);
  _tft->print("PROPAGATION");
  
  _tft->setFreeFont(nullptr);
  _tft->setTextSize(1);
  
  // Determine if it's daytime
  bool isDay = true;
  if (timeValid) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      isDay = (timeinfo.tm_hour >= 6 && timeinfo.tm_hour < 18);
    }
  }
  
  // Solar indices at top
  _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  _tft->setCursor(20, 30);
  _tft->print("Solar Indices:");
  
  _tft->setTextColor(TFT_WHITE, TFT_BLACK);
  _tft->setCursor(30, 42);
  _tft->printf("SFI: %3d  SSN: %3d  A: %2d  K: %2d  X-Ray: %s",
               solarData.sfi, solarData.ssn, solarData.aIndex, 
               solarData.kIndex, solarData.xRay.c_str());
  
  // Band conditions - two columns (Day | Night)
  String bands[] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"};
  
  int col1_x = 30;
  int col2_x = 170;
  int y_start = 65;
  int row_height = 14;
  
  // Column headers
  _tft->setTextColor(TFT_CYAN, TFT_BLACK);
  _tft->setCursor(col1_x + 30, y_start - 10);
  _tft->print("DAY");
  _tft->setCursor(col2_x + 30, y_start - 10);
  _tft->print("NIGHT");
  
  for (int i = 0; i < 10; i++) {
    int y = y_start + (i * row_height);
    
    // Band label
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(col1_x - 10, y + 2);
    _tft->print(bands[i]);
    
    // Day condition
    BandCondition dayCondition = calculateBandCondition(bands[i], solarData, true);
    uint16_t dayColor = conditionToColor(dayCondition);
    _tft->fillRect(col1_x + 25, y, 60, 12, dayColor);
    _tft->setTextColor(TFT_BLACK, dayColor);
    _tft->setCursor(col1_x + 30, y + 2);
    switch (dayCondition) {
      case BAND_EXCELLENT: _tft->print("EXCELLENT"); break;
      case BAND_GOOD:      _tft->print("GOOD"); break;
      case BAND_FAIR:      _tft->print("FAIR"); break;
      case BAND_POOR:      _tft->print("POOR"); break;
    }
    
    // Night condition
    BandCondition nightCondition = calculateBandCondition(bands[i], solarData, false);
    uint16_t nightColor = conditionToColor(nightCondition);
    _tft->fillRect(col2_x + 25, y, 60, 12, nightColor);
    _tft->setTextColor(TFT_BLACK, nightColor);
    _tft->setCursor(col2_x + 30, y + 2);
    switch (nightCondition) {
      case BAND_EXCELLENT: _tft->print("EXCELLENT"); break;
      case BAND_GOOD:      _tft->print("GOOD"); break;
      case BAND_FAIR:      _tft->print("FAIR"); break;
      case BAND_POOR:      _tft->print("POOR"); break;
    }
  }
  
  // Current time indicator
//  if (timeValid) {
//    struct tm timeinfo;
//    if (getLocalTime(&timeinfo)) {
//      char timeStr[32];
//      strftime(timeStr, sizeof(timeStr), "%H:%M UTC", &timeinfo);
//      _tft->setTextColor(TFT_CYAN, TFT_BLACK);
//      _tft->setCursor(10, 230);
//      _tft->printf("Current: %s (%s)", timeStr, isDay ? "DAY" : "NIGHT");
//    }
//  }

  if (timeValid) {
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(10, 230);
    _tft->print(timeStr);  // Use pre-formatted string
  }

}

// ============================================================================
// PAGE 4: SOLAR DATA (Detailed)
// ============================================================================

void DisplayManager::drawSolarPage(SolarIndices solarData, bool timeValid, const char* timeStr) {
  static bool pageInitialized = false;
//  static unsigned long lastFullUpdate = 0;
  
  if (!pageInitialized) {
    _tft->fillScreen(TFT_BLACK);
    pageInitialized = true;
  }
  
  // Throttle updates
//  if (millis() - lastFullUpdate < 1000 && pageInitialized) {
//    return;
//  }
//  lastFullUpdate = millis();
  
  // Title
  _tft->setFreeFont(&DINPro_Regular15pt8b);
  _tft->setTextColor(TFT_CYAN, TFT_BLACK);
  _tft->setCursor(10, 20);
  _tft->print("SOLAR DATA");
  
  _tft->setFreeFont(&DINPro_Regular6pt8b);
  
  int y = 40;
  int col1 = 20;
  int col2 = 190;
  
  // Solar Flux Index
  _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  _tft->setCursor(col1, y);
  _tft->print("Solar Flux Index (SFI):");
  _tft->setTextColor(TFT_WHITE, TFT_BLACK);
  _tft->setCursor(col2, y);
  _tft->printf("%d", solarData.sfi);
  y += 14;
  
  _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
  _tft->setCursor(col1 + 10, y);
  _tft->print("10.7cm radio flux");
  y += 17;
  
  // Sunspot Number
  _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  _tft->setCursor(col1, y);
  _tft->print("Sunspot Number (SSN):");
  _tft->setTextColor(TFT_WHITE, TFT_BLACK);
  _tft->setCursor(col2, y);
  _tft->printf("%d", solarData.ssn);
  y += 14;
  
  _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
  _tft->setCursor(col1 + 10, y);
  _tft->print("Daily sunspot count");
  y += 17;
  
  // A-Index
  _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  _tft->setCursor(col1, y);
  _tft->print("A-Index:");
  _tft->setTextColor(TFT_WHITE, TFT_BLACK);
  _tft->setCursor(col2, y);
  _tft->printf("%d", solarData.aIndex);
  y += 14;
  
  _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
  _tft->setCursor(col1 + 10, y);
  _tft->print("Geomagnetic activity (0-400)");
  y += 17;
  
  // K-Index
  _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  _tft->setCursor(col1, y);
  _tft->print("K-Index:");
  _tft->setTextColor(TFT_WHITE, TFT_BLACK);
  _tft->setCursor(col2, y);
  _tft->printf("%d", solarData.kIndex);
  y += 14;
  
  _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
  _tft->setCursor(col1 + 10, y);
  _tft->print("Geomagnetic activity (0-9)");
  y += 17;
  
  // X-Ray Flux
  _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  _tft->setCursor(col1, y);
  _tft->print("X-Ray Flux:");
  _tft->setTextColor(TFT_WHITE, TFT_BLACK);
  _tft->setCursor(col2, y);
  _tft->print(solarData.xRay);
  y += 14;
  
  _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
  _tft->setCursor(col1 + 10, y);
  _tft->print("Solar flare indicator");
  y += 17;
  
  // Bz (Solar Wind)
  _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  _tft->setCursor(col1, y);
  _tft->print("Bz (Solar Wind):");
  
  if (solarData.bz < 0) {
    _tft->setTextColor(TFT_RED, TFT_BLACK);
  } else {
    _tft->setTextColor(TFT_GREEN, TFT_BLACK);
  }
  _tft->setCursor(col2, y);
  _tft->printf("%.1f nT", solarData.bz);
  y += 14;
  
  _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
  _tft->setCursor(col1 + 10, y);
  _tft->print("Negative = disturbed");
  
  // Clear timestamp area and redraw
  _tft->fillRect(10, 220, 130, 15, TFT_BLACK);
  
  // Clear data area and redraw
  _tft->fillRect(188, 30, 110, 180, TFT_BLACK);
  
  if (timeValid) {
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(10, 230);
    _tft->print(timeStr);  // Use pre-formatted string
  }
 
  _tft->setFreeFont(nullptr);
}

// ============================================================================
// PAGE 5: PSK REPORTER
// ============================================================================

void DisplayManager::drawPSKPage(PSKReporter* pskReporter, bool timeValid, const char* timeStr) {
  static bool pageInitialized = false;
  
  if (!pageInitialized) {
    _tft->fillScreen(TFT_BLACK);
    pageInitialized = true;
  }
  
  // Title
  _tft->setFreeFont(&DINPro_Regular15pt8b);
  _tft->setTextColor(TFT_CYAN, TFT_BLACK);
  _tft->setCursor(10, 20);
  _tft->print("PSK REPORTER");
  
  _tft->setFreeFont(&DINPro_Regular6pt8b);
  
  if (pskReporter && pskReporter->isDataValid()) {
    int spotCount = 0;
    PSKSpot* allSpots = pskReporter->getSpots(spotCount);
    String mostActiveBand = pskReporter->getMostActiveBand();
    
    // Clear area and redraw
    _tft->fillRect(5, 25, 265, 175, TFT_BLACK);
    
    _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    _tft->setCursor(20, 40);
    _tft->printf("Most Active: %s", mostActiveBand.c_str());
    
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(180, 40);
    _tft->printf("Total: %d", spotCount);
    
    // Show top 10 spots from most active band
    if (spotCount > 0 && mostActiveBand != "None") {
      _tft->setTextColor(TFT_CYAN, TFT_BLACK);
      _tft->setCursor(10, 60);
      _tft->print("Recent Spots:");
      
      int y = 75;
      int displayedSpots = 0;
      
      // Find and display up to 10 spots from the most active band
      for (int i = 0; i < spotCount && displayedSpots < 10; i++) {
        if (allSpots[i].band == mostActiveBand) {

          // TX Call
          _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
          _tft->setCursor(10, y);
          _tft->printf("%.6s", allSpots[i].txCallsign.c_str());
          
          // Arrow
          _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
          _tft->setCursor(60, y);
          _tft->print(">");
          
          // RX Call
          _tft->setTextColor(TFT_GREEN, TFT_BLACK);
          _tft->setCursor(70, y);
          _tft->printf("%.6s", allSpots[i].rxCallsign.c_str());
          
          // SNR
          _tft->setTextColor(allSpots[i].snr > 0 ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
          _tft->setCursor(130, y);
          _tft->printf("%+3d", allSpots[i].snr);
          
          // Mode
          _tft->setTextColor(TFT_CYAN, TFT_BLACK);
          _tft->setCursor(165, y);
          _tft->printf("%.4s", allSpots[i].mode.c_str());
          
          // Distance
          if (allSpots[i].distance > 0) {
            _tft->setTextColor(TFT_WHITE, TFT_BLACK);
            _tft->setCursor(210, y);
            if (allSpots[i].distance < 1000) {
              _tft->printf("%3.0fkm", allSpots[i].distance);
            } else {
              _tft->printf("%.1fk", allSpots[i].distance / 1000.0);
            }
          }
          
          y += 13;
          displayedSpots++;
        }
      }
      
      if (displayedSpots == 0) {
        _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
        _tft->setCursor(40, 85);
        _tft->print("No spots on this band");
      }
      
    } else {
         _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
         _tft->setCursor(40, 80);
         _tft->print("No activity detected");
    }
    
  } else {
          _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
          _tft->setCursor(40, 80);
          _tft->print("Loading PSK Reporter data...");
  }
  
  // Clear timestamp area and redraw
  _tft->fillRect(10, 220, 130, 15, TFT_BLACK);
  
  if (timeValid) {
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(10, 230);
    _tft->print(timeStr);
  }
  
  _tft->setFreeFont(nullptr);
}

// ============================================================================
// PAGE 6: WEATHER
// ============================================================================
void DisplayManager::drawWeatherPage(WeatherInfo weather, bool timeValid, const char* timeStr) {
  static bool pageInitialized = false;
  
  if (!pageInitialized) {
    _tft->fillScreen(TFT_BLACK);
    pageInitialized = true;
  }
  
  // Title
  _tft->setFreeFont(&DINPro_Regular15pt8b);
  _tft->setTextColor(TFT_CYAN, TFT_BLACK);
  _tft->setCursor(10, 20);
  _tft->print("WEATHER");

  
  if (weather.valid) {
    // Clear the Temp position area before redrawing
    _tft->fillRect(80, 35, 90, 32, TFT_BLACK);
   
    // Temperature - large
    _tft->setFreeFont(&DINPro_Regular15pt8b);
    _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    _tft->setCursor(100, 60);
    _tft->printf("%.0f", weather.tempF);
    _tft->print("\xB0");      // Degree symbol (string hex)

    // Switch to smaller font for details
    _tft->setFreeFont(&DINPro_Regular6pt8b);
    
    int y = 80;
    int labelX = 20;
    int valueX = 180;
   
   // Clear the Stats position area before redrawing
    _tft->fillRect(175, 70, 90, 135, TFT_BLACK);  
    
    // Feels like
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(labelX, y);
    _tft->print("Feels like:");
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(valueX, y);
    _tft->printf("%.0f", weather.feelsLikeF);
    _tft->print("\xB0");      // Degree symbol (string hex)
    y += 15;
    
    // Min / Max
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(labelX, y);
    _tft->print("Min / Max:");
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(valueX, y);
    _tft->printf("%.0f", weather.tempMinF);
    _tft->print("\xB0");      // Degree symbol (string hex)
    _tft->print(" / ");
    _tft->printf("%.0f", weather.tempMaxF);
    _tft->print("\xB0");      // Degree symbol (string hex)
    y += 15;
    
    // Humidity
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(labelX, y);
    _tft->print("Humidity:");
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(valueX, y);
    _tft->printf("%d%%", weather.humidity);
    y += 15;
    
    // Pressure
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(labelX, y);
    _tft->print("Pressure:");
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(valueX, y);
    _tft->printf("%.2f inHg", weather.pressure);
    y += 15;
    
    // Wind / Gust
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(labelX, y);
    _tft->print("Wind / Gust:");
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(valueX, y);
    _tft->printf("%.0f mph", weather.windSpeed);
    if (weather.windGust > 0) {
      _tft->printf(" / %.0f", weather.windGust);
    }
    y += 15;
    
    // Direction
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(labelX, y);
    _tft->print("Direction:");
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(valueX, y);
    _tft->print(weather.windDir);
    y += 15;
    
    // Visibility
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(labelX, y);
    _tft->print("Visibility:");
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(valueX, y);
    _tft->printf("%d m", weather.visibility);
    y += 15;
    
    // Sunrise (convert from unix timestamp to local time)
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(labelX, y);
    _tft->print("Sunrise:");
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(valueX, y);
    if (weather.sunrise > 0) {
      time_t sunriseTime = weather.sunrise + weather.timezoneOffsetSeconds;
      struct tm* sunriseInfo = gmtime(&sunriseTime);
      _tft->printf("%02d:%02d", sunriseInfo->tm_hour, sunriseInfo->tm_min);
    } else {
      _tft->print("--:--");
    }
    y += 15;
    
    // Sunset
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(labelX, y);
    _tft->print("Sunset:");
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(valueX, y);
    if (weather.sunset > 0) {
      time_t sunsetTime = weather.sunset + weather.timezoneOffsetSeconds;
      struct tm* sunsetInfo = gmtime(&sunsetTime);
      _tft->printf("%02d:%02d", sunsetInfo->tm_hour, sunsetInfo->tm_min);
    } else {
      _tft->print("--:--");
    }
    
  } else {
    _tft->setFreeFont(&DINPro_Regular6pt8b);
    _tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    _tft->setCursor(40, 100);
    _tft->print("Loading weather data...");
  }
  
  // Clear timestamp area and redraw
  _tft->fillRect(10, 220, 130, 15, TFT_BLACK);
  
  if (timeValid) {
    _tft->setTextColor(TFT_CYAN, TFT_BLACK);
    _tft->setCursor(10, 230);
    _tft->print(timeStr);
  }
  
  _tft->setFreeFont(nullptr);
}


// ============================================================================
// PAGE 7: SYSTEM STATS (Always Last)
// ============================================================================

void DisplayManager::drawStatsPage(bool wifiConnected, bool timeValid, const char* timeStr,
                                   SolarIndices solarData, WeatherInfo weather, 
                                   PSKReporter* pskReporter, float issLat, float issLon) {
  static bool pageInitialized = false;
//  static unsigned long lastFullUpdate = 0;
  
  if (!pageInitialized) {
    _tft->fillScreen(TFT_BLACK);
    pageInitialized = true;
  }
  
  // Throttle updates
//  if (millis() - lastFullUpdate < 1000 && pageInitialized) {
//    return;
//  }
//  lastFullUpdate = millis();
  
  // Title
  _tft->setTextColor(TFT_CYAN, TFT_BLACK);
  _tft->setCursor(10, 20);
  _tft->setFreeFont(&DINPro_Regular15pt8b);
  _tft->print("SYSTEM STATS");
  
  _tft->setFreeFont(&DINPro_Regular6pt8b);
  
  int y = 40;
  int col1 = 20;
  
  _tft->setTextColor(TFT_WHITE, TFT_BLACK);
  
  // Clear Uptime area for changing values
  _tft->fillRect(col1+43, y-10, 80, 15, TFT_BLACK);
  
  // Uptime
  unsigned long uptime = (millis() - _bootTime) / 1000;
  int days = uptime / 86400;
  int hours = (uptime % 86400) / 3600;
  int minutes = (uptime % 3600) / 60;
  _tft->setCursor(col1, y);
  _tft->printf("Uptime: %dd %02dh %02dm", days, hours, minutes);
  y += 15;
  
  if (wifiConnected) {
    _tft->setCursor(col1, y);
    _tft->printf("IP: %s", WiFi.localIP().toString().c_str());
    y += 15;
    
    _tft->setCursor(col1, y);
    _tft->printf("SSID: %s", WiFi.SSID().c_str());
    y += 15;
    
    _tft->setCursor(col1, y);
    _tft->printf("MAC: %s", WiFi.macAddress().c_str());
    y += 15;
    
    _tft->setCursor(col1, y);
    _tft->printf("Gateway: %s", WiFi.gatewayIP().toString().c_str());
    y += 15;
    
    _tft->setCursor(col1, y);
    _tft->printf("Subnet: %s", WiFi.subnetMask().toString().c_str());
    y += 15;
    
    _tft->setCursor(col1, y);
    _tft->printf("DNS: %s", WiFi.dnsIP().toString().c_str());
    y += 15;
    
    _tft->setCursor(col1, y);
    _tft->print("Host: hamclock.local");
    y += 15;
    
    // Clear Signal area before redrawing
    _tft->fillRect(col1+39, y-10, 60, 12, TFT_BLACK);
    
    int rssi = WiFi.RSSI();
    _tft->setCursor(col1, y);
    _tft->printf("Signal: %d dBm", rssi);
    y += 15;
  }
  
  _tft->setCursor(col1, y);
  _tft->printf("Chip Model: %s", ESP.getChipModel());
  y += 15;
  
  // Clear Free RAM area before redrawing
  _tft->fillRect(col1+55, y-10, 85, 12, TFT_BLACK);
  
  _tft->setCursor(col1, y);
  _tft->printf("Free RAM: %d bytes", ESP.getFreeHeap());
  y += 15;
  
  _tft->setCursor(col1, y);
  _tft->printf("CPU: %d MHz", ESP.getCpuFreqMHz());


  // Clear Time Display area for changing values
  _tft->fillRect(10, 220, 130, 15, TFT_BLACK);
  
  // Time display at bottom
  if (timeValid) {
//    struct tm timeinfo;
//    if (getLocalTime(&timeinfo)) {
//      char timeStr[32];
//      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
      _tft->setTextColor(TFT_CYAN, TFT_BLACK);
      _tft->setCursor(10, 230);
      _tft->print(timeStr);
//    }
  }

  // Clear bottom status area
  _tft->fillRect(210, 220, 110, 15, TFT_BLACK);
  
  // WiFi Status at bottom
  _tft->setCursor(210, 230);
  if (wifiConnected) {
    _tft->setTextColor(TFT_GREEN, TFT_BLACK);
    _tft->print("WiFi: Connected");
  } else {
    _tft->setTextColor(TFT_RED, TFT_BLACK);
    _tft->print("WiFi: Disconnected");
  }
  
  _tft->setFreeFont(nullptr);
}

// ============================================================================
// HELPER FUNCTIONS (kept from original)
// ============================================================================

void DisplayManager::drawMap() {
  _tft->pushImage(0, 0, MAP_WIDTH, MAP_HEIGHT, world_map);
}
