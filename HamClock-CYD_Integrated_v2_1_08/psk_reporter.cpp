// ============================================================================
// FILE: psk_reporter.cpp
// ============================================================================
#include <Arduino.h>
#include "psk_reporter.h"
#include "config.h"
#include <math.h>

PSKReporter::PSKReporter(const char* callsign) 
  : _callsign(callsign), _spotCount(0), _lastFetch(0), _dataValid(false), _lastUpdateTime(0) {}

void PSKReporter::update(bool wifiConnected) {
  if (!wifiConnected) return;
  
  // Update every 5 minutes
  if (_lastFetch == 0 || (millis() - _lastFetch > PSK_FETCH_INTERVAL)) {
    _lastFetch = millis();
    fetchFromAPI();
  }
}

void PSKReporter::fetchFromAPI() {
  Serial.println("Fetching PSK Reporter data...");
  
  // PSK Reporter Query API endpoint (returns XML)
  String url = "https://retrieve.pskreporter.info/query?";
  url += "flowStartSeconds=-900";  // Last 15 minutes
  url += "&rronly=1";
  url += "&noactive=1";
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  http.begin(client, url);
  http.setTimeout(10000);
  
  int httpCode = http.GET();
  
  Serial.print("HTTP Response: ");
  Serial.println(httpCode);
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    Serial.println("Parsing XML data...");
    
    _spotCount = 0;
    
    // Simple XML parsing - look for <receptionReport> tags
    int startPos = 0;
    while (startPos < payload.length() && _spotCount < MAX_PSK_SPOTS) {
      int tagStart = payload.indexOf("<receptionReport", startPos);
      if (tagStart == -1) break;
      
      int tagEnd = payload.indexOf("/>", tagStart);
      if (tagEnd == -1) break;
      
      String reportTag = payload.substring(tagStart, tagEnd + 2);
      
      // Extract attributes
      PSKSpot spot;
      spot.rxCallsign = extractAttribute(reportTag, "receiverCallsign");
      spot.txCallsign = extractAttribute(reportTag, "senderCallsign");
      spot.rxGrid = extractAttribute(reportTag, "receiverLocator");
      spot.txGrid = extractAttribute(reportTag, "senderLocator");
      spot.frequency = extractAttribute(reportTag, "frequency").toInt();
      spot.band = frequencyToBand(spot.frequency);
      spot.mode = extractAttribute(reportTag, "mode");
      spot.snr = extractAttribute(reportTag, "sNR").toInt();
      spot.timestamp = extractAttribute(reportTag, "flowStartSeconds").toInt();
      
      // Calculate distance
      if (spot.txGrid.length() >= 4 && spot.rxGrid.length() >= 4) {
        spot.distance = gridDistance(spot.txGrid, spot.rxGrid);
      } else {
        spot.distance = 0;
      }
      
      // Filter: only include spots with your callsign
      if (_callsign.length() > 0) {
        if (spot.txCallsign.indexOf(_callsign) >= 0 || 
            spot.rxCallsign.indexOf(_callsign) >= 0) {
          _spots[_spotCount++] = spot;
        }
      } else {
        // No filter - include all spots (up to MAX)
        _spots[_spotCount++] = spot;
      }
      
      startPos = tagEnd + 2;
    }
    
    _dataValid = true;
    time(&_lastUpdateTime);  // Record update time
    
    if (_callsign.length() > 0) {
      Serial.printf("PSK Reporter: %d spots involving %s\n", _spotCount, _callsign.c_str());
    } else {
      Serial.printf("PSK Reporter: %d total spots\n", _spotCount);
    }
    
  } else {
    Serial.print("PSK Reporter HTTP error: ");
    Serial.println(httpCode);
    _dataValid = false;
  }
  
  http.end();
}

String PSKReporter::extractAttribute(String xml, String attrName) {
  String searchStr = attrName + "=\"";
  int startPos = xml.indexOf(searchStr);
  if (startPos == -1) return "";
  
  startPos += searchStr.length();
  int endPos = xml.indexOf("\"", startPos);
  if (endPos == -1) return "";
  
  return xml.substring(startPos, endPos);
}

String PSKReporter::frequencyToBand(int freqHz) {
  float freqMHz = freqHz / 1000000.0;
  
  if (freqMHz >= 1.8 && freqMHz < 2.0) return "160m";
  if (freqMHz >= 3.5 && freqMHz < 4.0) return "80m";
  if (freqMHz >= 5.3 && freqMHz < 5.4) return "60m";
  if (freqMHz >= 7.0 && freqMHz < 7.3) return "40m";
  if (freqMHz >= 10.1 && freqMHz < 10.15) return "30m";
  if (freqMHz >= 14.0 && freqMHz < 14.35) return "20m";
  if (freqMHz >= 18.068 && freqMHz < 18.168) return "17m";
  if (freqMHz >= 21.0 && freqMHz < 21.45) return "15m";
  if (freqMHz >= 24.89 && freqMHz < 24.99) return "12m";
  if (freqMHz >= 28.0 && freqMHz < 29.7) return "10m";
  if (freqMHz >= 50.0 && freqMHz < 54.0) return "6m";
  
  return "?";
}

float PSKReporter::gridDistance(String grid1, String grid2) {
  if (grid1.length() < 4 || grid2.length() < 4) return 0;
  
  float lat1, lon1, lat2, lon2;
  gridToLatLon(grid1, lat1, lon1);
  gridToLatLon(grid2, lat2, lon2);
  
  // Haversine formula for great circle distance
  float dLat = radians(lat2 - lat1);
  float dLon = radians(lon2 - lon1);
  float a = sin(dLat/2) * sin(dLat/2) +
            cos(radians(lat1)) * cos(radians(lat2)) *
            sin(dLon/2) * sin(dLon/2);
  float c = 2 * atan2(sqrt(a), sqrt(1-a));
  return 6371.0 * c;  // Earth radius in km
}

void PSKReporter::gridToLatLon(String grid, float &lat, float &lon) {
  grid.toUpperCase();
  
  // Convert Maidenhead grid to lat/lon (center of square)
  lon = (grid[0] - 'A') * 20 - 180;
  lat = (grid[1] - 'A') * 10 - 90;
  
  if (grid.length() >= 4) {
    lon += (grid[2] - '0') * 2;
    lat += (grid[3] - '0') * 1;
  }
  
  if (grid.length() >= 6) {
    lon += (grid[4] - 'A') * (2.0/24.0);
    lat += (grid[5] - 'A') * (1.0/24.0);
  }
  
  // Center of grid square
  lon += 1;
  lat += 0.5;
}

PSKSpot* PSKReporter::getSpots(int &count) {
  count = _spotCount;
  return _spots;
}

int PSKReporter::getSpotCount() {
  return _spotCount;
}

bool PSKReporter::isDataValid() {
  return _dataValid;
}

BandActivity PSKReporter::getBandActivity(String band) {
  BandActivity activity;
  activity.band = band;
  activity.spotCount = 0;
  activity.avgSNR = 0;
  activity.active = false;
  
  int totalSNR = 0;
  for (int i = 0; i < _spotCount; i++) {
    if (_spots[i].band == band) {
      activity.spotCount++;
      totalSNR += _spots[i].snr;
    }
  }
  
  if (activity.spotCount > 0) {
    activity.avgSNR = totalSNR / activity.spotCount;
    activity.active = true;
  }
  
  return activity;
}

String PSKReporter::getMostActiveBand() {
  String bands[] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"};
  int maxSpots = 0;
  String mostActive = "None";
  
  for (int i = 0; i < 10; i++) {
    BandActivity activity = getBandActivity(bands[i]);
    if (activity.spotCount > maxSpots) {
      maxSpots = activity.spotCount;
      mostActive = bands[i];
    }
  }
  
  return mostActive;
}

int PSKReporter::getTotalSpots() {
  return _spotCount;
}

String PSKReporter::getLastUpdateString(int timezoneOffsetSeconds) {
  if (_lastUpdateTime == 0) {
    return "Updated: --:--";
  }
  
  time_t localTime = _lastUpdateTime + timezoneOffsetSeconds;
  struct tm* timeInfo = gmtime(&localTime);
  
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "Updated: %02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  return String(buffer);
}
