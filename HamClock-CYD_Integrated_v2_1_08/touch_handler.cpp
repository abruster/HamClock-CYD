// ============================================================================
// FILE: touch_handler.cpp
// ============================================================================
// Implementation of touch screen handler for E32R28T
// See touch_handler.h for detailed documentation
// ============================================================================

#include <Arduino.h>
#include "touch_handler.h"

// ============================================================================
// HARDWARE CONFIGURATION - E32R28T 2.8" ESP32 Board
// ============================================================================
// Touch pins (HSPI bus - separate from TFT)
#define TOUCH_CS   33    // Chip Select
#define TOUCH_IRQ  36    // Interrupt (not used)
#define TOUCH_MOSI 32    // Master Out Slave In
#define TOUCH_MISO 39    // Master In Slave Out  
#define TOUCH_SCK  25    // Clock

// ============================================================================
// CALIBRATION VALUES
// ============================================================================
// These values map raw touch coordinates to screen pixels
// Obtained from running touch calibration sketch
// Format: setCal(460, 3378, 744, 3496, 320, 240, 1)
//         setCal(hmin, hmax, vmin, vmax, width, height, xyswap)
#define TOUCH_MIN_X 460
#define TOUCH_MAX_X 3378
#define TOUCH_MIN_Y 744
#define TOUCH_MAX_Y 3496

// Screen dimensions in landscape mode (rotation 1)
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Touch pressure threshold
// Real touches on resistive screens typically have z > 300
// Lower values may cause false triggers
#define TOUCH_PRESSURE_THRESHOLD 300

// ============================================================================
// CONSTRUCTOR
// ============================================================================
TouchHandler::TouchHandler(TFT_eSPI* tft) 
  : _tft(tft), 
    _currentMode(MODE_MAIN), 
    _lastTouch(0), 
    _touched(false), 
    _touchX(0), 
    _touchY(0) {
  
  // Initialize separate SPI bus for touch (HSPI)
  // NOTE: Must use HSPI because TFT uses VSPI on this board
  _touchSPI = new SPIClass(HSPI);
  _touchSPI->begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  
  // Initialize touchscreen object with CS pin
  _ts = new XPT2046_Touchscreen(TOUCH_CS);
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void TouchHandler::begin() {
  // Initialize touchscreen with the separate SPI bus
  if (_ts->begin(*_touchSPI)) {
    Serial.println("Touch handler initialized successfully");
    Serial.println("Touch library: XPT2046_Touchscreen");
    Serial.printf("Calibration: X(%d-%d) Y(%d-%d)\n", 
                  TOUCH_MIN_X, TOUCH_MAX_X, TOUCH_MIN_Y, TOUCH_MAX_Y);
  } else {
    Serial.println("ERROR: Touchscreen initialization failed!");
    Serial.println("Check wiring and SPI bus configuration");
  }
}

// ============================================================================
// TOUCH DETECTION AND HANDLING
// ============================================================================
bool TouchHandler::checkTouch() {
  // Check if screen is currently being touched
  if (_ts->touched()) {
    
    // Get touch point with coordinates and pressure
    TS_Point p = _ts->getPoint();
    
    // Validate touch by checking pressure threshold
    if (p.z > TOUCH_PRESSURE_THRESHOLD) {
      
      // Debounce: Ignore touches too close together
      if (millis() - _lastTouch > TOUCH_DEBOUNCE) {
        _lastTouch = millis();
        
        // Map raw touch coordinates to screen pixels
        _touchX = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
        _touchY = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);
        
        // Clamp to screen boundaries
        _touchX = constrain(_touchX, 0, SCREEN_WIDTH - 1);
        _touchY = constrain(_touchY, 0, SCREEN_HEIGHT - 1);
        
        _touched = true;
        
        Serial.printf("Touch: raw(%d,%d) → screen(%d,%d) pressure=%d\n", 
                      p.x, p.y, _touchX, _touchY, p.z);
        
        // LEFT THIRD = Previous page
        if (_touchX < 107) {
          int prevMode = (int)_currentMode - 1;
          if (prevMode < 0) {
            prevMode = MODE_COUNT - 1;  // Wrap to last page (STATS)
          }
          _currentMode = (DisplayMode)prevMode;
          Serial.printf("→ Previous page: Mode %d\n", _currentMode);
        }
        // RIGHT THIRD = Next page
        else if (_touchX > 213) {
          int nextMode = (int)_currentMode + 1;
          if (nextMode >= MODE_COUNT) {
            nextMode = 0;  // Wrap to first page (MAIN)
          }
          _currentMode = (DisplayMode)nextMode;
          Serial.printf("→ Next page: Mode %d\n", _currentMode);
        }
        // MIDDLE THIRD = No action (could be used for page-specific actions later)
        else {
          Serial.println("→ Middle touch (no action)");
        }
        
        return true;  // Touch was processed
      }
    }
  }
  
  return false;  // No valid touch detected
}

// ============================================================================
// GETTERS
// ============================================================================
DisplayMode TouchHandler::getCurrentMode() {
  return _currentMode;
}

void TouchHandler::setMode(DisplayMode mode) {
  _currentMode = mode;
  Serial.printf("Mode manually set to: %d\n", mode);
}

uint16_t TouchHandler::getTouchX() {
  return _touchX;
}

uint16_t TouchHandler::getTouchY() {
  return _touchY;
}
