// ============================================================================
// FILE: grayline.cpp
// ============================================================================
#include <Arduino.h>
#include <math.h>
#include "grayline.h"

Grayline::Grayline() : _solarDeclination(0), _equationOfTime(0), _lastUpdate(0) {}

void Grayline::update(time_t currentTime) {
  tmElements_t tm;
  breakTime(currentTime, tm);
  
  int dayOfYear = tm.Day;
  for (int m = 1; m < tm.Month; m++) {
    if (m == 2) dayOfYear += 28;
    else if (m == 4 || m == 6 || m == 9 || m == 11) dayOfYear += 30;
    else dayOfYear += 31;
  }
  
  _solarDeclination = calculateSolarDeclination(dayOfYear);
  _equationOfTime = calculateEquationOfTime(dayOfYear);
  _lastUpdate = currentTime;
  
  Serial.print("Solar declination: ");
  Serial.print(_solarDeclination);
  Serial.print("° | Subsolar lon: ");
  Serial.println(getSubsolarLongitude(currentTime));
}

bool Grayline::needsRedraw(time_t currentTime) {
  // Force draw on first call or every 10 minutes
  if (_lastUpdate == 0) return true;
  return (currentTime - _lastUpdate) >= 600;
}

void Grayline::forceRedraw() {
  _lastUpdate = 0;  // Reset timer to force next redraw
}


float Grayline::calculateSolarDeclination(int dayOfYear) {
  float n = dayOfYear;
  float declination = -23.45 * cos(radians((360.0 / 365.0) * (n + 10)));
  return declination;
}

float Grayline::calculateEquationOfTime(int dayOfYear) {
  float n = dayOfYear;
  float b = radians((360.0 / 365.0) * (n - 81));
  float eot = 9.87 * sin(2 * b) - 7.53 * cos(b) - 1.5 * sin(b);
  return eot;
}

float Grayline::getSubsolarLatitude() {
  return _solarDeclination;
}

float Grayline::getSubsolarLongitude(time_t currentTime) {
  tmElements_t tm;
  breakTime(currentTime, tm);
  
  float utcHours = tm.Hour + (tm.Minute / 60.0) + (tm.Second / 3600.0);
  float solarHours = utcHours + (_equationOfTime / 60.0);
  float longitude = (12.0 - solarHours) * 15.0;
  
  while (longitude > 180.0) longitude -= 360.0;
  while (longitude < -180.0) longitude += 360.0;
  
  return longitude;
}

int Grayline::getYForX(int x, int mapWidth, int mapHeight) {
  float lon = map(x, 0, mapWidth - 1, -180.0 * 100, 180.0 * 100) / 100.0;
  
  time_t now;
  time(&now);
  
  float subsolarLon = getSubsolarLongitude(now);
  float subsolarLat = getSubsolarLatitude();
  
  float hourAngle = lon - subsolarLon;
  
  while (hourAngle > 180.0) hourAngle -= 360.0;
  while (hourAngle < -180.0) hourAngle += 360.0;
  
  float terminatorLat;
  
  float cosHA = cos(radians(hourAngle));
  if (abs(cosHA) < 0.001) {
    terminatorLat = (hourAngle > 0) ? 90.0 : -90.0;
  } else {
    float tanDec = tan(radians(subsolarLat));
    terminatorLat = degrees(atan(-tanDec / cosHA));
  }
  
  if (terminatorLat > 90.0) terminatorLat = 90.0;
  if (terminatorLat < -90.0) terminatorLat = -90.0;
  
  int y = map(terminatorLat * 100, 90.0 * 100, -90.0 * 100, 0, mapHeight - 1);
  
  return y;
}

bool Grayline::isDaylight(float lat, float lon) {
  time_t now;
  time(&now);
  
  float subsolarLon = getSubsolarLongitude(now);
  float subsolarLat = getSubsolarLatitude();
  
  float dLon = radians(lon - subsolarLon);
  float lat1 = radians(subsolarLat);
  float lat2 = radians(lat);
  
  float cosAngle = sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(dLon);
  
  if (cosAngle > 1.0) cosAngle = 1.0;
  if (cosAngle < -1.0) cosAngle = -1.0;
  
  float angle = degrees(acos(cosAngle));
  
  return angle < 90.0;
}
