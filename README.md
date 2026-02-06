# HamClock-CYD

A comprehensive ESP32-based amateur radio station display system that integrates real-time propagation data, ISS tracking, solar conditions, weather information, and PSK Reporter data into a unified touch-screen interface.

![HamClock-CYD Banner](docs/banner.jpg)

## Overview

HamClock-CYD is an all-in-one amateur radio workstation display designed for the ESP32-32E (E32R28T) development board with a 2.8" ILI9341 touchscreen display. This project combines embedded programming, RF engineering knowledge, and web technologies to create a functional ham radio tool that displays critical information for radio operators in real-time.

## Features

### Core Functionality
- **Seven-Page Touch Navigation System**
  - Main world map with grayline visualization
  - ISS position tracking with pass predictions
  - Real-time propagation data and band conditions
  - Solar conditions (SFI, A-index, K-index, sunspot numbers)
  - PSK Reporter propagation monitoring
  - Weather conditions and forecasts
  - System statistics and diagnostics

### Intelligent Display Management
- **Dual Digital Clocks** - Local time and UTC with 7-segment font display
- **Scrolling Weather Banner** - Configurable speed with current conditions
- **Optimized Redraw Logic** - Updates only when data changes, preventing UI sluggishness
- **Touch-Responsive Navigation** - Smooth page transitions with minimal latency

### Configuration Management
- **Three Operating Modes**
  - **SETUP Mode** - WiFi configuration via QR code display
  - **CONFIGURATION Mode** - Web-based settings management
  - **RUNNING Mode** - Normal HamClock operation
- **Web Interface** - Comprehensive configuration portal accessible via browser
- **Persistent Storage** - WiFi credentials in NVS, display preferences in SPIFFS JSON

### Data Integration
- Real-time ISS tracking via N2YO API
- Solar propagation data from HamQSL.com
- Weather information from OpenWeatherMap
- PSK Reporter real-time propagation conditions
- VHF propagation openings monitoring

## Hardware Requirements

### Required Components
- **ESP32 Board**: Hoysond ESP32-32E (E32R28T) or compatible ESP32 development board
- **Display**: 2.8" ILI9341 TFT LCD (240x320 pixels)
- **Touch Controller**: XPT2046 resistive touch controller
- **Power Supply**: USB or 5V power source

### Pin Configuration
The system uses ESP32's three separate SPI buses:
- **Display (VSPI)**: Standard SPI pins for ILI9341
- **Touch**: Separate GPIO pins for XPT2046 controller

See `hardware_config.h` for detailed pin mappings.

## Software Dependencies

### Arduino Libraries
- `TFT_eSPI` - Display control for ILI9341
- `XPT2046_Touchscreen` - Touch functionality
- `WiFi` - ESP32 WiFi connectivity
- `WebServer` - Configuration web interface
- `HTTPClient` - API data fetching
- `ArduinoJson` - JSON parsing and configuration
- `Preferences` (NVS) - Persistent WiFi storage
- `SPIFFS` - Web file storage
- `AlFonts` - Custom typography library

### API Keys Required
- **N2YO API** - For ISS tracking data (get free key at https://www.n2yo.com/api/)
- **OpenWeatherMap API** - For weather information (get free key at https://openweathermap.org/api)

## Installation

### 1. Arduino IDE Setup
```bash
# Install ESP32 board support in Arduino IDE
# Board Manager URL: https://dl.espressif.com/dl/package_esp32_index.json
# Select Board: "ESP32 Dev Module"
```

### 2. Library Installation
Install required libraries via Arduino Library Manager or manually:
- TFT_eSPI (configured for ILI9341)
- XPT2046_Touchscreen
- ArduinoJson (v6.x or later)

### 3. TFT_eSPI Configuration
Edit `TFT_eSPI/User_Setup.h` to match your display:
```cpp
#define ILI9341_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
// Configure pins to match your board
```

### 4. Upload Sketch
1. Clone this repository
2. Open `HamClock-CYD.ino` in Arduino IDE
3. Configure your location in `config.h`:
   ```cpp
   #define HOME_LAT 37.564541
   #define HOME_LON -97.352284
   ```
4. Add your API keys to `secrets.h`:
   ```cpp
   #define N2YO_API_KEY "your_key_here"
   #define OPENWEATHER_API_KEY "your_key_here"
   ```
5. Upload to ESP32

### 5. SPIFFS Upload
Upload web interface files:
```bash
# Arduino IDE: Tools > ESP32 Sketch Data Upload
# Or use esptool.py for manual SPIFFS upload
```

## Configuration

### First-Time Setup
1. **WiFi Configuration** - On first boot, HamClock displays a QR code
2. Scan QR code to access setup portal
3. Enter WiFi credentials and save
4. System automatically restarts in RUNNING mode

### Web Configuration Interface
Access the web interface at `http://[ESP32_IP_ADDRESS]` to configure:
- Display preferences (brightness, colors, page order)
- Weather location and units
- Solar data refresh intervals
- PSK Reporter filters
- Touch calibration

### Touch Calibration
If touch response is inaccurate:
1. Access web interface
2. Navigate to "Touch Calibration"
3. Follow on-screen prompts to tap calibration points
4. Save new calibration values

## Usage

### Navigation
- **Swipe Left/Right** - Navigate between pages
- **Tap Buttons** - Access specific functions
- **Long Press** - Access configuration mode (if enabled)

### Page Descriptions
1. **Main Map** - World map with grayline, ISS position, and home location
2. **ISS Tracking** - Detailed satellite tracking with pass predictions
3. **Propagation** - Band conditions and solar indices
4. **Solar Conditions** - SFI, A/K indices, sunspot numbers, solar flux
5. **PSK Reporter** - Real-time propagation spots and paths
6. **Weather** - Current conditions and multi-day forecast
7. **System Stats** - CPU usage, memory, WiFi signal, uptime

### Data Refresh Intervals
- ISS position: Every 60 seconds
- Solar data: Every 15 minutes
- Weather: Every 30 minutes
- PSK Reporter: Every 5 minutes
- Clock: Every second

## Development

### Project Structure
```
HamClock-CYD/
├── HamClock-CYD.ino          # Main sketch
├── config.h                   # User configuration
├── secrets.h                  # API keys (not in repo)
├── wifi_manager.h/cpp         # WiFi management
├── iss_tracker.h/cpp          # ISS tracking module
├── solar_data.h/cpp           # Solar propagation data
├── weather_data.h/cpp         # Weather integration
├── psk_reporter.h/cpp         # PSK Reporter module
├── display_manager.h/cpp      # Display control
├── touch_handler.h/cpp        # Touch input processing
├── web_server.h/cpp           # Configuration portal
├── data/                      # SPIFFS web files
│   ├── index.html
│   ├── config.js
│   └── styles.css
└── fonts/                     # Custom font files
```

### Building Custom Fonts
To create custom fonts for the display:
```bash
# Using Adafruit fontconvert in WSL/Linux
cd fonts/
./fontconvert SourceFile.ttf 24 32 126 > CustomFont24.h
```

### Debugging
Enable serial debugging in `config.h`:
```cpp
#define DEBUG_MODE 1
#define SERIAL_BAUD 115200
```

Monitor output via Serial Monitor at 115200 baud for diagnostic information.

## Performance Optimization

### Display Updates
The system uses intelligent redraw logic:
- Only updates display regions that have changed
- Clock updates optimized to once per second
- Data updates only on API refresh or value changes
- Touch response prioritized over display updates

### Memory Management
- FreeRTOS tasks for background operations
- Efficient string handling to minimize heap fragmentation
- Periodic heap monitoring and cleanup
- SPIFFS for web files, NVS for critical config only

## Attributions

This project incorporates code and concepts from the following excellent projects:

- **HamQSL XML Parser** by canislupus11  
  https://github.com/canislupus11/HamQSL-XML-Parser  
  *Solar data parsing and propagation information integration*

- **ESP32 2.8 Inch TFT HamClock** by HB9IIU  
  https://github.com/HB9IIU/ESP32-28-Inch-TFT-HamClock  
  *Display layout concepts and touch interface design*

- **ESP32-CYD-HamClock** by HB9IIU  
  https://github.com/HB9IIU/ESP32-CYD-HamClock  
  *ESP32 hardware configuration and display optimization techniques*

Special thanks to these developers for their pioneering work in ESP32-based ham radio displays.

## License

This project is released under the MIT License. See `LICENSE` file for details.

Portions of this code are derived from the projects listed in Attributions above, which are also MIT licensed.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for:
- Bug fixes
- Feature enhancements
- Documentation improvements
- Hardware compatibility reports

### Development Guidelines
- Follow existing code style and organization
- Test thoroughly on actual hardware before submitting
- Update documentation for new features
- Include comments for complex logic

## Support

### Getting Help
- **Issues**: Report bugs or request features via GitHub Issues
- **Discussions**: Join project discussions for general questions
- **Documentation**: See `/docs` folder for detailed guides

### Known Issues
- Touch calibration may drift over time in extreme temperatures
- Large font rendering can cause brief display flicker
- SD card support planned but not yet implemented

## Roadmap

### Planned Features
- [ ] POTA (Parks on the Air) spot integration
- [ ] Historical data tracking and graphing
- [ ] SD card support for logging
- [ ] Additional VHF propagation enhancements
- [ ] Band condition matrix with day/night columns
- [ ] Customizable page layouts
- [ ] Multiple weather location support
- [ ] APRS integration

### Future Enhancements
- Mobile app companion for remote configuration
- MQTT support for home automation integration
- Additional API data sources
- Custom alert system for propagation events

## Gallery

*(Add screenshots of your HamClock display here)*

## Changelog

See `CHANGELOG.md` for version history and release notes.

## Contact

**Project Maintainer**: abruster  
**Location**: Wichita, Kansas, USA (Grid: EM17)  
**Coordinates**: 37.8003°N, 97.5250°W

## Acknowledgments

- Amateur radio community for continuous inspiration
- ESP32 development community for excellent documentation
- Arduino platform for accessible embedded development
- All contributors and testers who help improve this project

---

**73 and happy DXing!** 📻🌍

*HamClock-CYD - Bringing real-time propagation data to your shack, one ESP32 at a time.*
