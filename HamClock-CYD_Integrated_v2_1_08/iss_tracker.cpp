// ============================================================================
// FILE: iss_tracker.cpp (ENHANCED with Pass Predictions)
// ============================================================================
#include <Arduino.h>
#include "iss_tracker.h"
#include "config.h"

// ADD THIS LINE - allows ISS tracker to access the config structure:
extern HamClockConfig config;

ISSTracker::ISSTracker() 
  : _latitude(0.0), _longitude(0.0), _userLat(0.0), _userLon(0.0),
    _lastPosFetch(0), _lastPassFetch(0), 
    _posDataValid(false), _passDataValid(false), _passCount(0) {
  for (int i = 0; i < 3; i++) {
    _passes[i].isValid = false;
  }
}

void ISSTracker::setUserLocation(float lat, float lon) {
  _userLat = lat;
  _userLon = lon;
  Serial.printf("ISS Tracker: User location set to %.4f, %.4f\n", lat, lon);
}

void ISSTracker::update(bool wifiConnected) {
  if (!wifiConnected) return;
  
  // Update ISS position every 5 seconds
  if (millis() - _lastPosFetch >= ISS_FETCH_INTERVAL) {
    _lastPosFetch = millis();

    HTTPClient http;
    http.begin("https://api.wheretheiss.at/v1/satellites/25544");

    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      StaticJsonDocument<512> doc;
      
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        _latitude = doc["latitude"].as<float>();
        _longitude = doc["longitude"].as<float>();
        _posDataValid = true;
        
        Serial.printf("ISS: %.4f, %.4f\n", _latitude, _longitude);
      }
    }
    http.end();
  }
  // 1 hr = 3600000 millis, 6 hrs = 21600000 millis   
  // Update pass predictions every 6 hours (or if never fetched)
  if (millis() - _lastPassFetch >= 3600000 || _lastPassFetch == 0) {
    fetchPassPredictions();
  }
}

void ISSTracker::fetchPassPredictions() {
  if (_userLat == 0.0 && _userLon == 0.0) {
    Serial.println("ISS Pass: No user location set");
    return;
  }
  
  _lastPassFetch = millis();

  // Debug what config we're seeing
  Serial.println("🔍 fetchPassPredictions() - Checking config:");
  Serial.printf("  config.n2yoApiKey: '%s'\n", config.n2yoApiKey);
  Serial.printf("  config.n2yoApiKeyValid: %s\n", config.n2yoApiKeyValid ? "true" : "false");
  Serial.printf("  strlen(config.n2yoApiKey): %d\n", strlen(config.n2yoApiKey));
  
  // Determine which location to use
  float lat = (config.n2yoLatitude != 0.0) ? config.n2yoLatitude : _userLat;
  float lon = (config.n2yoLongitude != 0.0) ? config.n2yoLongitude : _userLon;
  
  // ============================================================================
  // AUTOMATIC API SELECTION WITH FALLBACK
  // ============================================================================
  
  // Try N2YO if API key is provided
  if (config.n2yoApiKeyValid && strlen(config.n2yoApiKey) > 0) {
    Serial.println("ISS Pass: Using N2YO API (detailed predictions)");
    
    String url = "https://api.n2yo.com/rest/v1/satellite/visualpasses/25544/";
    url += String(lat, 4) + "/" + String(lon, 4) + "/0/5/300/";
//    url += config.n2yoApiKey;
    url += "?apiKey=" + String(config.n2yoApiKey);  // Changed to query parameter
    
    Serial.println("📡 N2YO URL: " + url);
    
    HTTPClient http;
    http.setTimeout(15000);
    http.begin(url);
    
    int httpCode = http.GET();
    Serial.printf("📡 N2YO HTTP Response: %d\n", httpCode);
    
    if (httpCode == 200) {
      String payload = http.getString();
      // ADD THIS DEBUG LINE:
      Serial.println("📦 N2YO Response: " + payload);
      
      DynamicJsonDocument doc(2048);
      
      DeserializationError error = deserializeJson(doc, payload);
if (!error) {
  _passCount = doc["info"]["passescount"].as<int>();
  if (_passCount > 3) _passCount = 3;  // Limit to 3
  
  if (_passCount > 0) {
    JsonArray passArray = doc["passes"];
    
    for (int i = 0; i < _passCount; i++) {
      JsonObject pass = passArray[i];
      
      _passes[i].riseTime = pass["startUTC"].as<unsigned long>();
      _passes[i].maxTime = pass["maxUTC"].as<unsigned long>();
      _passes[i].setTime = pass["endUTC"].as<unsigned long>();
      _passes[i].maxElevation = pass["maxEl"].as<float>();
      _passes[i].magnitude = pass["mag"].as<float>();
//      _passes[i].duration = pass["duration"].as<int>();
      
      const char* startDir = pass["startAzCompass"];
      strncpy(_passes[i].direction, startDir, sizeof(_passes[i].direction) - 1);
      _passes[i].direction[sizeof(_passes[i].direction) - 1] = '\0';
      
      const char* endDir = pass["endAzCompass"];
      strncpy(_passes[i].endDirection, endDir, sizeof(_passes[i].endDirection) - 1);
      _passes[i].endDirection[sizeof(_passes[i].endDirection) - 1] = '\0';
      
      _passes[i].isValid = true;
    }
    
    _passDataValid = true;
               
        Serial.println("✅ N2YO: Pass prediction received successfully");
        http.end();
        return;  // Success! Exit function
      }  // ADD THIS - closes if (_passCount > 0)
      }  // ADD THIS - closes if (!error)
      } else {  // Now this makes sense - else for if (httpCode == 200)
  Serial.printf("⚠ N2YO HTTP error: %d\n", httpCode);
      }
      http.end();     // ADD THIS
      }               // ADD THIS - closes if (config.n2yoApiKeyValid...)
      else {          // ADD THIS  
        Serial.println("ISS Pass: No N2YO API key - using Open-Notify");
}               // ADD THIS
  
  // OPTION 2: Using Open-Notify API (No API key needed, but less detailed)
  // Uncomment this section to use Open-Notify instead:
  
  // FALLBACK: Open-Notify API
  String url2 = "http://api.open-notify.org/iss-pass.json?lat=";  // Changed to url2
  url2 += String(_userLat, 4) + "&lon=" + String(_userLon, 4);     // url2
  url2 += "&alt=100&n=1";

  Serial.println("📡 ISS Pass URL: " + url2);                      // url2

  HTTPClient http2;                                                 // Changed to http2
  http2.begin(url2);                                                // http2, url2

  int httpCode2 = http2.GET();                                      // Changed to httpCode2
  if (httpCode2 == 200) {                                           // httpCode2
    String payload = http2.getString();                             // http2
     StaticJsonDocument<512> doc;
    
    DeserializationError error = deserializeJson(doc, payload);
    if (!error && doc["response"].size() > 0) {
      JsonObject pass = doc["response"][0];
      
      _passes[0].riseTime = pass["risetime"].as<unsigned long>();
      _passes[0].maxTime = _passes[0].riseTime + (pass["duration"].as<int>() / 2);
      _passes[0].setTime = _passes[0].riseTime + pass["duration"].as<int>();
      _passes[0].maxElevation = 0;  // Open-Notify doesn't provide this
      _passes[0].magnitude = 0;     // Open-Notify doesn't provide this
      strcpy(_passes[0].direction, "N/A");  // Open-Notify doesn't provide this
      
      _passes[0].isValid = true;
      _passDataValid = true;
      
      Serial.println("ISS Pass prediction fetched (Open-Notify)");
    } else {
      Serial.println("ISS Pass: No upcoming visible passes");
      _passes[0].isValid = false;
    }
  } else {
    Serial.printf("ISS Pass HTTP error: %d\n", httpCode2);
  }
  http2.end();
}

float ISSTracker::getLatitude() { return _latitude; }
float ISSTracker::getLongitude() { return _longitude; }
bool ISSTracker::isDataValid() { return _posDataValid; }
bool ISSTracker::hasPassData() { return _passDataValid && _passCount > 0; }
ISSPass ISSTracker::getNextPass() { return _passes[0]; }
// ADD THESE TWO NEW FUNCTIONS:
int ISSTracker::getPassCount() { 
  return _passCount; 
}

ISSPass ISSTracker::getPass(int index) { 
  if (index >= 0 && index < _passCount && index < 3) {
    return _passes[index];
  }
  ISSPass invalid;
  invalid.isValid = false;
  return invalid;
}
