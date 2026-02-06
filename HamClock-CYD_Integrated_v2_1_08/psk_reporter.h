// ============================================================================
// NEW FILE: psk_reporter.h
// ============================================================================
// PSK Reporter integration for real-time propagation data
// ============================================================================
#ifndef PSK_REPORTER_H
#define PSK_REPORTER_H

#include <HTTPClient.h>
#include <ArduinoJson.h>

// Maximum number of recent spots to track
#define MAX_PSK_SPOTS 50

struct PSKSpot {
  String txCallsign;      // Transmitting callsign
  String rxCallsign;      // Receiving callsign
  String txGrid;          // Transmitter grid square
  String rxGrid;          // Receiver grid square
  String band;            // Band (e.g., "20m")
  int frequency;          // Frequency in Hz
  String mode;            // Mode (e.g., "FT8", "FT4")
  int snr;                // Signal-to-noise ratio
  time_t timestamp;       // When spotted
  float distance;         // Distance in km
};

struct BandActivity {
  String band;
  int spotCount;
  int avgSNR;
  bool active;
};

class PSKReporter {
  private:
    PSKSpot _spots[MAX_PSK_SPOTS];
    int _spotCount;
    unsigned long _lastFetch;
    bool _dataValid;
    time_t _lastUpdateTime;  // Timestamp of last successful data update
    String _callsign;
    
    // Helper to extract band from frequency
    String frequencyToBand(int freqHz);
    
    // Calculate distance between two grid squares
    float gridDistance(String grid1, String grid2);
    
    // Convert grid square to lat/lon
    void gridToLatLon(String grid, float &lat, float &lon);
    String extractAttribute(String xml, String attrName);  // ADD THIS LINE
  
  public:
    PSKReporter(const char* callsign);
    void update(bool wifiConnected);
    void fetchFromAPI();
    
    // Get spot data
    PSKSpot* getSpots(int &count);
    int getSpotCount();
    bool isDataValid();
    String getLastUpdateString(int timezoneOffsetSeconds);  // Get formatted update time
    
    // Get band activity summary
    BandActivity getBandActivity(String band);
    String getMostActiveBand();
    int getTotalSpots();
};

#endif
