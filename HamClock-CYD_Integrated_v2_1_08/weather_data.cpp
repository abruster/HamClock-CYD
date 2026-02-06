// ============================================================================
// UPDATED FILE: weather_data.cpp
// Now supports BOTH city/country AND lat/lon modes
// ============================================================================
#include <Arduino.h>
#include "weather_data.h"
#include "config.h"

// Global variables to override with lat/lon (set from main sketch)
float g_weatherLat = 0.0;
float g_weatherLon = 0.0;

WeatherData::WeatherData(const char* apiKey, const char* city, const char* country) 
  : _apiKey(apiKey), _city(city), _country(country), _lastFetch(0), _dataValid(false), _lastUpdateTime(0) {
  _weather.valid = false;
  _weather.tempF = 0;
  _weather.humidity = 0;
  _weather.pressure = 0;
  _weather.windSpeed = 0;
  _weather.windDeg = 0;
  _weather.windDir = "N";
  _weather.description = "Loading...";
  _weather.icon = "01d";
  _weather.cityName = "";
  _weather.timezoneOffsetSeconds;
}

void WeatherData::update(bool wifiConnected) {
  if (!wifiConnected) return;
  
  // Force fetch on first call or after interval (10 minutes)
  if (_lastFetch == 0 || (millis() - _lastFetch > WEATHER_FETCH_INTERVAL)) {
    _lastFetch = millis();
    fetchFromAPI();
  }
}

void WeatherData::fetchFromAPI() {
  Serial.println("Fetching weather data from OpenWeatherMap...");
  
  // Build API URL - use lat/lon if available, otherwise city/country
  String url = "http://api.openweathermap.org/data/2.5/weather?";
  
  if (g_weatherLat != 0.0 && g_weatherLon != 0.0) {
    // Use lat/lon mode
    url += "lat=" + String(g_weatherLat, 6);
    url += "&lon=" + String(g_weatherLon, 6);
    Serial.printf("Using lat/lon: %.6f, %.6f\n", g_weatherLat, g_weatherLon);
  } else {
    // Use city/country mode
    url += "q=" + String(_city) + "," + String(_country);
    Serial.printf("Using city: %s, %s\n", _city, _country);
  }
  
  url += "&appid=";
  url += _apiKey;
  url += "&units=imperial";  // Use imperial for Fahrenheit
  
  Serial.println("Weather URL: " + url);
  
  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    // Parse JSON response
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      // Extract weather data
      _weather.tempF = doc["main"]["temp"].as<float>();
      _weather.feelsLikeF = doc["main"]["feels_like"].as<float>();  // ADD
      _weather.tempMinF = doc["main"]["temp_min"].as<float>();      // ADD
      _weather.tempMaxF = doc["main"]["temp_max"].as<float>();      // ADD
      _weather.humidity = doc["main"]["humidity"].as<int>();
      _weather.pressure = mbarToInHg(doc["main"]["pressure"].as<float>());
      _weather.windSpeed = doc["wind"]["speed"].as<float>();
      _weather.windGust = doc["wind"]["gust"] | 0.0;  // ADD (may not always be present)
      _weather.windDeg = doc["wind"]["deg"].as<int>();
      _weather.windDir = degreesToDirection(_weather.windDeg);
      _weather.description = doc["weather"][0]["main"].as<String>();
      _weather.icon = doc["weather"][0]["icon"].as<String>();
      _weather.cityName = doc["name"].as<String>();
      _weather.country = doc["sys"]["country"].as<String>();        // ADD
      _weather.visibility = doc["visibility"] | 0;                  // ADD
      _weather.sunrise = doc["sys"]["sunrise"].as<long>();          // ADD
      _weather.sunset = doc["sys"]["sunset"].as<long>();            // ADD
      _weather.timezoneOffsetSeconds = doc["timezone"].as<int>();
      _weather.valid = true;
      time(&_lastUpdateTime);  // Record update time
      _dataValid = true;
      
      Serial.println("Weather data updated:");
      Serial.printf("  City: %s, %s\n", _weather.cityName.c_str(), _weather.country.c_str());
      Serial.printf("  Temp: %.1f°F (Feels like: %.1f°F)\n", _weather.tempF, _weather.feelsLikeF);
      Serial.printf("  Min/Max: %.1f°F / %.1f°F\n", _weather.tempMinF, _weather.tempMaxF);
      Serial.printf("  Humidity: %d%%\n", _weather.humidity);
      Serial.printf("  Pressure: %.2f inHg\n", _weather.pressure);
      Serial.printf("  Wind: %s @ %.1f mph, Gust: %.1f mph\n", 
                    _weather.windDir.c_str(), _weather.windSpeed, _weather.windGust);
      Serial.printf("  Conditions: %s\n", _weather.description.c_str());
      Serial.printf("  Visibility: %d m\n", _weather.visibility);
    } else {
      Serial.printf("  Timezone offset: %d seconds (%d hours)\n", _weather.timezoneOffsetSeconds, _weather.timezoneOffsetSeconds/3600);
      Serial.println("Weather JSON parse failed");
      _weather.valid = false;
    }
  } else {
    Serial.print("Weather HTTP error: ");
    Serial.println(httpCode);
    if (httpCode == 401) {
      Serial.println("Check your OpenWeatherMap API key!");
    } else if (httpCode == 404) {
      Serial.println("City not found or invalid coordinates!");
    }
    _weather.valid = false;
  }
  
  http.end();
}

String WeatherData::degreesToDirection(int deg) {
  const char* directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int index = ((deg + 22) / 45) % 8;
  return directions[index];
}

float WeatherData::mbarToInHg(float mbar) {
  return mbar * 0.02953;  // Conversion factor
}

WeatherInfo WeatherData::getWeather() {
  return _weather;
}

bool WeatherData::isDataValid() {
  return _dataValid && _weather.valid;
}

String WeatherData::getLastUpdateString(int timezoneOffsetSeconds) {
  if (_lastUpdateTime == 0) {
    return "Updated: --:--";
  }
  
  time_t localTime = _lastUpdateTime + timezoneOffsetSeconds;
  struct tm* timeInfo = gmtime(&localTime);
  
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "Updated: %02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  return String(buffer);
}
