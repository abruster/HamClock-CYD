// ============================================================================
// FILE: touch_handler.h
// ============================================================================
// Touch screen handler for E32R28T 2.8" ESP32 display
// Uses XPT2046_Touchscreen library (not TFT_Touch)
// 
// HARDWARE NOTES:
// - Touch controller: XPT2046 (resistive touch)
// - Touch uses SEPARATE SPI bus (HSPI) from TFT display
// - TFT is on VSPI, Touch is on HSPI - they do NOT share the same SPI bus
// - Touch pressure threshold: Real touches typically > 300
//
// PIN CONNECTIONS (E32R28T board):
//   TOUCH_CS   = 33  (Chip Select)
//   TOUCH_IRQ  = 36  (Interrupt - not used in this implementation)
//   TOUCH_MOSI = 32  (Master Out Slave In)
//   TOUCH_MISO = 39  (Master In Slave Out)
//   TOUCH_SCK  = 25  (Clock)
//
// CALIBRATION VALUES (from your calibration sketch):
//   X_MIN = 460,  X_MAX = 3378
//   Y_MIN = 744,  Y_MAX = 3496
//   Screen: 320x240 in landscape mode (rotation 1)
//
// USAGE EXAMPLE:
//   #include "touch_handler.h"
//   
//   TFT_eSPI tft = TFT_eSPI();
//   TouchHandler touch(&tft);
//   
//   void setup() {
//     tft.init();
//     tft.setRotation(1);
//     touch.begin();
//   }
//   
//   void loop() {
//     if (touch.checkTouch()) {
//       // Touch detected, mode changed
//       DisplayMode mode = touch.getCurrentMode();
//       if (mode == MODE_STATS) {
//         // Show stats screen
//       }
//     }
//   }
// ============================================================================

#ifndef TOUCH_HANDLER_H
#define TOUCH_HANDLER_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// Display modes that can be cycled through by touching
enum DisplayMode {
  MODE_MAIN = 0,        // Main display (map + band conditions)
  MODE_ISS,             // ISS Position
  MODE_PROPAGATION,     // Propagation (band conditions day/night)
  MODE_SOLAR,           // Solar data details
  MODE_PSK,             // PSK Reporter
  MODE_WEATHER,         // Weather details
  MODE_STATS,           // System statistics (always last)
  MODE_COUNT            // Total number of modes
};

class TouchHandler {
  private:
    TFT_eSPI* _tft;                    // Pointer to TFT display object
    XPT2046_Touchscreen* _ts;          // Pointer to touchscreen object
    SPIClass* _touchSPI;               // Separate SPI bus for touch
    DisplayMode _currentMode;          // Current display mode
    unsigned long _lastTouch;          // Last touch time (for debounce)
    bool _touched;                     // Touch state flag
    uint16_t _touchX;                  // Last touch X coordinate (mapped)
    uint16_t _touchY;                  // Last touch Y coordinate (mapped)
    
    const unsigned long TOUCH_DEBOUNCE = 300;  // Debounce delay in milliseconds

  public:
    // Constructor - pass pointer to TFT display object
    TouchHandler(TFT_eSPI* tft);
    
    // Initialize touch screen (call in setup())
    void begin();
    
    // Check for touch and handle mode changes
    // Returns true if touch detected, false otherwise
    bool checkTouch();
    
    // Get current display mode
    DisplayMode getCurrentMode();
    
    // Manually set display mode (if needed)
    void setMode(DisplayMode mode);
    
    // Get last touch X coordinate (0-320 in landscape)
    uint16_t getTouchX();
    
    // Get last touch Y coordinate (0-240 in landscape)
    uint16_t getTouchY();
};

#endif
