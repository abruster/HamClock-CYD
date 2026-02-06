// ============================================================================
// FILE: grayline.h
// ============================================================================
#ifndef GRAYLINE_H
#define GRAYLINE_H

#include <TimeLib.h>

class Grayline {
  private:
    float _solarDeclination;
    float _equationOfTime;
    time_t _lastUpdate;
    
    float calculateSolarDeclination(int dayOfYear);
    float calculateEquationOfTime(int dayOfYear);
    float getSubsolarLatitude();
    float getSubsolarLongitude(time_t currentTime);

  public:
    Grayline();
    void update(time_t currentTime);
    bool needsRedraw(time_t currentTime);
    void forceRedraw();  // ADD THIS
    int getYForX(int x, int mapWidth, int mapHeight);
    bool isDaylight(float lat, float lon);
};

#endif
