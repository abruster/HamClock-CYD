// ============================================================================
// FILE: wifi_manager.cpp
// ============================================================================
#include <Arduino.h>
#include "wifi_manager.h"
#include "config.h"

WiFiManager::WiFiManager(const char* ssid, const char* password) 
  : _ssid(ssid), _password(password), _connected(false), 
    _timeValid(false), _mdnsStarted(false), _lastWifiAttempt(0), _lastNTPSync(0) {}

void WiFiManager::begin() {
  WiFi.mode(WIFI_STA);
  Serial.println("WiFi Manager initialized");
}

void WiFiManager::startMDNS() {
  if (_mdnsStarted) return;
  
  // Start mDNS with hostname
  if (MDNS.begin("hamclock")) {
    _mdnsStarted = true;
    Serial.println("mDNS responder started");
    Serial.println("Access via: http://hamclock.local");
    
    // Add service advertisement for HTTP
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("Error starting mDNS responder!");
  }
}

void WiFiManager::update() {
  // Check WiFi status
  if (WiFi.status() != WL_CONNECTED) {
    _connected = false;
    _mdnsStarted = false;  // Reset mDNS flag when WiFi disconnects
    
    if (millis() - _lastWifiAttempt > WIFI_RETRY_MS) {
      _lastWifiAttempt = millis();
      Serial.println("Attempting WiFi connection...");
      WiFi.begin(_ssid, _password);
    }
    return;
  }

  // WiFi just connected
  if (!_connected) {
    _connected = true;
    Serial.println("WiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    // Start mDNS service
    startMDNS();
    
    // Configure NTP
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, 
               "pool.ntp.org", "time.nist.gov");
    _lastNTPSync = 0;
  }

  // Sync time
  if (!_timeValid || millis() - _lastNTPSync > NTP_RETRY_MS) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      _timeValid = true;
      _lastNTPSync = millis();
      Serial.println("Time synchronized!");
    }
  }
}

bool WiFiManager::isConnected() { return _connected; }
bool WiFiManager::isTimeValid() { return _timeValid; }

String WiFiManager::getHostname() {
  return "hamclock.local";
}
