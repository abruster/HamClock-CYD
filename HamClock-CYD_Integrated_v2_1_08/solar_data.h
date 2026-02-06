// ============================================================================
// FILE: solar_data.h
// ============================================================================
#ifndef SOLAR_DATA_H
#define SOLAR_DATA_H

struct SolarIndices {
  int sfi;        // Solar Flux Index (SFU)
  int ssn;        // Sunspot Number
  int aIndex;     // A-index (geomagnetic activity, daily)
  int kIndex;     // K-index (geomagnetic activity, 3-hour)
  String xRay;    // X-Ray flux class (e.g., "C2.1", "M1.5")
  float bz;       // Solar wind Bz (nT) - negative is bad for aurora
};

class SolarData {
  private:
    SolarIndices _indices;
    unsigned long _lastFetch;
    bool _dataValid;
    time_t _lastUpdateTime;  // Timestamp of last successful data update
    
    // Helper function to parse XML
    int extractXMLValue(String xml, String tag);
	String extractXMLString(String xml, String tag);  // ADD THIS
	
  public:
    SolarData();
    void update(bool wifiConnected);
    SolarIndices getIndices();
    bool isDataValid();
    String getLastUpdateString(int timezoneOffsetSeconds);  // Get formatted update time
    
    void fetchFromAPI();
};

#endif
