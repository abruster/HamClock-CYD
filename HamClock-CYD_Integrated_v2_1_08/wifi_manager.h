// ============================================================================
// FILE: wifi_manager.h
// ============================================================================
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <ESPmDNS.h>  // ADD THIS LINE
#include <time.h>

class WiFiManager {
  private:
    bool _connected;
    bool _timeValid;
	bool _mdnsStarted;  // ADD THIS LINE
    unsigned long _lastWifiAttempt;
    unsigned long _lastNTPSync;
    const char* _ssid;
    const char* _password;
	
	 void startMDNS();  // ADD THIS LINE

  public:
    WiFiManager(const char* ssid, const char* password);
    void begin();
    void update();
    bool isConnected();
    bool isTimeValid();
	String getHostname();  // ADD THIS LINE - returns the mDNS hostname
};

#endif
