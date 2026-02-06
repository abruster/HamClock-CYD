// ============================================================================
// NEW FILE: weather_data.h
// ============================================================================
#ifndef WEATHER_DATA_H
#define WEATHER_DATA_H

#include <HTTPClient.h>
#include <ArduinoJson.h>

struct WeatherInfo {
  float tempF;              // Temperature in Fahrenheit
  float feelsLikeF;         // ADD THIS - Feels like temperature
  float tempMinF;           // ADD THIS - Min temperature
  float tempMaxF;           // ADD THIS - Max temperature
  int humidity;             // Humidity percentage
  float pressure;           // Pressure in inHg
  float windSpeed;          // Wind speed in mph
  float windGust;           // ADD THIS - Wind gust in mph
  int windDeg;              // Wind direction in degrees
  String windDir;           // Wind direction (N, NE, E, etc.)
  String description;       // Weather description
  String icon;              // Weather icon code
  String cityName;          // City name
  String country;           // ADD THIS - Country code
  int visibility;           // ADD THIS - Visibility in meters
  long sunrise;             // ADD THIS - Sunrise time (unix timestamp)
  long sunset;              // ADD THIS - Sunset time (unix timestamp)
  int timezoneOffsetSeconds; // Timezone offset from UTC in seconds
  bool valid;               // Data validity flag
};

class WeatherData {
  private:
    WeatherInfo _weather;
    unsigned long _lastFetch;
    bool _dataValid;
    time_t _lastUpdateTime;  // Timestamp of last successful data update
    const char* _apiKey;
    const char* _city;
    const char* _country;
    
    // Helper to convert wind degrees to direction
    String degreesToDirection(int deg);
    
    // Helper to convert mbar to inHg
    float mbarToInHg(float mbar);

  public:
    WeatherData(const char* apiKey, const char* city, const char* country);
    void update(bool wifiConnected);
    void fetchFromAPI();
    WeatherInfo getWeather();
    bool isDataValid();
    String getLastUpdateString(int timezoneOffsetSeconds);  // Get formatted update time
};

#endif
