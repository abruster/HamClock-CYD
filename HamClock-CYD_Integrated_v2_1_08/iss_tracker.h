// ============================================================================
// FILE: iss_tracker.h (ENHANCED with Pass Predictions)
// ============================================================================
#ifndef ISS_TRACKER_H
#define ISS_TRACKER_H

#include <HTTPClient.h>
#include <ArduinoJson.h>

// Structure to hold ISS pass information
struct ISSPass {
  unsigned long riseTime;      // Unix timestamp
  unsigned long maxTime;       // Unix timestamp
  unsigned long setTime;       // Unix timestamp
  float maxElevation;          // Degrees above horizon
  float magnitude;             // Visual magnitude (brightness)
  int duration;                // Make sure this exists
  char direction[8];           // Rise direction (N, NE, E, etc)
  char endDirection[8];        // Make sure this exists too
  bool isValid;
};

class ISSTracker {
  private:
    float _latitude;
    float _longitude;
    float _userLat;              // User's location for pass predictions
    float _userLon;
    unsigned long _lastPosFetch;
    unsigned long _lastPassFetch;
    bool _posDataValid;
    bool _passDataValid;
    ISSPass _passes[3];  // ← Store 3 passes instead
    int _passCount;      // ← Track how many we have

    void fetchPassPredictions();
    
  public:
    ISSTracker();
    void setUserLocation(float lat, float lon);
    void update(bool wifiConnected);
    float getLatitude();
    float getLongitude();
    bool isDataValid();
    bool hasPassData();
    ISSPass getNextPass();
    int getPassCount();        // ADD THIS
    ISSPass getPass(int index); // ADD THIS
};

#endif
