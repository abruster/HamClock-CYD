// ============================================================================
// FILE: solar_data.cpp
// ============================================================================
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "solar_data.h"
#include "config.h"

SolarData::SolarData() : _lastFetch(0), _dataValid(false), _lastUpdateTime(0) {
  // Initialize with placeholder values
  _indices.sfi = 0;
  _indices.ssn = 0;
  _indices.aIndex = 0;
  _indices.kIndex = 0;
}

void SolarData::update(bool wifiConnected) {
  if (!wifiConnected) return;
  
  // Force fetch on first call or after interval
  if (_lastFetch == 0 || (millis() - _lastFetch > SOLAR_FETCH_INTERVAL)) {
    _lastFetch = millis();
    fetchFromAPI();
  }
}

void SolarData::fetchFromAPI() {
  Serial.println("Fetching solar data from HamQSL...");
  
  HTTPClient http;
  http.begin("https://www.hamqsl.com/solarxml.php");
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    // Parse XML data
    int sfiValue = extractXMLValue(payload, "solarflux");
    int ssnValue = extractXMLValue(payload, "sunspots");
    int aValue = extractXMLValue(payload, "aindex");
    int kValue = extractXMLValue(payload, "kindex");
    
    // Extract X-Ray flux
    String xRayValue = extractXMLString(payload, "xray");
    
 // Extract Bz (solar wind magnetic field)
 //   // Note: HamQSL provides this as "magneticfield" or we may need to parse it differently
 //   String bzString = extractXMLString(payload, "magneticfield");
 //   float bzValue = bzString.toFloat();

    // Extract Bz (solar wind magnetic field) - may be in different tags
    String bzString = extractXMLString(payload, "solarwind");
    if (bzString.length() == 0) {
      bzString = extractXMLString(payload, "magneticfield");
    }
    float bzValue = bzString.toFloat();


    
    // Update values if parsing was successful
    if (sfiValue >= 0) {
      _indices.sfi = sfiValue;
      _indices.ssn = ssnValue;
      _indices.aIndex = aValue;
      _indices.kIndex = kValue;
      _indices.xRay = xRayValue.length() > 0 ? xRayValue : "---";
      _indices.bz = bzValue;
      _dataValid = true;
      time(&_lastUpdateTime);  // Record update time
      
      Serial.println("Solar data updated:");
      Serial.printf("  SFI: %d\n", _indices.sfi);
      Serial.printf("  SSN: %d\n", _indices.ssn);
      Serial.printf("  A-index: %d\n", _indices.aIndex);
      Serial.printf("  K-index: %d\n", _indices.kIndex);
      Serial.printf("  X-Ray: %s\n", _indices.xRay.c_str());
      Serial.printf("  Bz: %.1f nT\n", _indices.bz);
    } else {
      Serial.println("Failed to parse solar data");
    }
    
  } else {
    Serial.print("Solar data HTTP error: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

// Helper function to extract integer value from XML tags
int SolarData::extractXMLValue(String xml, String tag) {
  String openTag = "<" + tag + ">";
  String closeTag = "</" + tag + ">";
  
  int startIndex = xml.indexOf(openTag);
  if (startIndex == -1) return -1;
  
  startIndex += openTag.length();
  int endIndex = xml.indexOf(closeTag, startIndex);
  if (endIndex == -1) return -1;
  
  String value = xml.substring(startIndex, endIndex);
  value.trim();
  
  return value.toInt();
}

// Helper function to extract string value from XML tags
String SolarData::extractXMLString(String xml, String tag) {
  String openTag = "<" + tag + ">";
  String closeTag = "</" + tag + ">";
  
  int startIndex = xml.indexOf(openTag);
  if (startIndex == -1) return "";
  
  startIndex += openTag.length();
  int endIndex = xml.indexOf(closeTag, startIndex);
  if (endIndex == -1) return "";
  
  String value = xml.substring(startIndex, endIndex);
  value.trim();
  
  return value;
}

SolarIndices SolarData::getIndices() { return _indices; }
bool SolarData::isDataValid() { return _dataValid; }

String SolarData::getLastUpdateString(int timezoneOffsetSeconds) {
  if (_lastUpdateTime == 0) {
    return "Updated: --:--";
  }
  
  time_t localTime = _lastUpdateTime + timezoneOffsetSeconds;
  struct tm* timeInfo = gmtime(&localTime);
  
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "Updated: %02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  return String(buffer);
}
