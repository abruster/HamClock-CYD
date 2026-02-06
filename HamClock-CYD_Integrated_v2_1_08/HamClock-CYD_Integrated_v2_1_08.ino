// ============================================================================
// HamClock-CYD Integrated System - Version v2_1_08
// WiFi Setup (QR) → Full Configuration (LittleFS HTML) → Running Mode
// All credentials loaded dynamically from LittleFS/NVS 
// Tools → Partition Scheme → "No OTA (2MB APP/2MB SPIFFS)" ←IMPORTANT !!! 
//============================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include "html_page.h"      // WiFi setup page
#include "html_success.h"   // Success page
#include "fonts/DINPro_Regular6pt8b.h"  //Extended character set includes degree symbol ° and stroke Ø
#include "fonts/DINPro_Regular11pt7b.h"
#include "fonts/DINPro_Regular15pt8b.h" //Extended character set includes degree symbol ° and stroke Ø
#include "fonts/DINPro_Regular18pt7b.h"

// HamClock main components
#include "config.h"         // Hardware constants and dummy defines
#include "iss_tracker.h"
#include "solar_data.h"
#include "display_manager.h"
#include "weather_data.h"
#include "psk_reporter.h"
#include "touch_handler.h"

// PNG decoder for splash screen images
#include <PNGdec.h>

// Include ESP32's built-in QRCode library
extern "C" {
  #include "qrcode.h"
}

// ============================================================================
// CONFIGURATION
// ============================================================================
#define AP_SSID_PREFIX  "HamClock-"
#define AP_PASSWORD     ""
#define AP_IP           IPAddress(192, 168, 4, 1)
#define AP_GATEWAY      IPAddress(192, 168, 4, 1)
#define AP_SUBNET       IPAddress(255, 255, 255, 0)

#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   240

// ============================================================================
// OPERATION MODES
// ============================================================================
enum OperationMode {
  MODE_SETUP,           // Initial WiFi setup with QR codes
  MODE_CONFIGURATION,   // Web-based configuration (LittleFS HTML primary)
  MODE_RUNNING          // Normal HamClock operation
};

OperationMode currentMode = MODE_SETUP;

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
TFT_eSPI tft = TFT_eSPI();
WebServer server(80);
Preferences prefs;

// WiFi Configuration
struct WiFiConfig {
  char ssid[32];
  char password[64];
  char timezone_iso[64];
  long timezone_offset;
  bool configured;
};

WiFiConfig wifiConfig;

// Declare config variable (no initialization here)
HamClockConfig config;

// Function to set defaults
void initConfigDefaults() {
  strcpy(config.apiKey, "");
  config.apiKeyValid = false;
  strcpy(config.n2yoApiKey, "");
  config.n2yoApiKeyValid = false;
  config.n2yoLatitude = 0.0;    // Will default to config.latitude if not set
  config.n2yoLongitude = 0.0;   // Will default to config.longitude if not set
  strcpy(config.callsign, "");
  strcpy(config.gridSquare, "");
  config.latitude = 0.0;
  config.longitude = 0.0;
  config.localTimeColour = 0x07E0;    // Green
  config.localFrameColour = 0xCFF9;   // Light Green
  config.utcTimeColour = 0xFFE0;      // Yellow
  config.utcFrameColour = 0xCE40;     // Light Yellow
  config.bannerColour = 0xA35A;       // Lavender
  config.italicClockFonts = false;
  config.doubleFrame = true;
  config.bannerSpeed = 23;  // ms between frames (10=fast, 120=slow)
  config.bannerPixelsPerFrame = 1;  // Always 1 for smooth scrolling (Hosyond-style)
  config.screenSaverTimeout = 7200000;
  config.autoPageChange = false;
  strcpy(config.startupLogo, "logo1.png");
  config.splashDuration = 3000;  // ms — minimum time splash stays on screen
  config.utcOffsetSeconds = 0;
}

// HamClock component pointers (initialized in RUNNING mode)
ISSTracker* issTracker = nullptr;
SolarData* solarData = nullptr;
WeatherData* weatherData = nullptr;
PSKReporter* pskReporter = nullptr;
DisplayManager* display = nullptr;

// Splash screen hold state — splash stays visible until first page draw
unsigned long splashDisplayedAt = 0;
bool splashActive = false;

// Background component update task — keeps blocking HTTP calls off the main loop
static volatile int pendingComponentUpdate = -1;  // -1 = idle, 0-3 = which component
TouchHandler* touch = nullptr;

// ============================================================================
// PNG DECODER GLOBALS (for splash screen)
// ============================================================================
PNG png;
File pngFile;  // Global file handle required by PNGdec callbacks

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== HamClock 🕰 Integrated System ===");
  
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed!");
    showError("LittleFS Failed");
    while(1) delay(1000);
  }
  Serial.println("LittleFS mounted successfully");
  listLittleFSFiles();
  
  // Initialize config with defaults
  initConfigDefaults();
  
  // DEBUG: Print what we just set
  Serial.println("🎨 After initConfigDefaults:");
  Serial.printf("  localTimeColour: 0x%04X\n", config.localTimeColour);
  Serial.printf("  utcTimeColour: 0x%04X\n", config.utcTimeColour);
  
  // Load configurations (will override defaults if file exists)
  loadWiFiConfig();
  loadHamClockConfig();
  
  // DEBUG: Print after loading
  Serial.println("🎨 After loadHamClockConfig:");
  Serial.printf("  localTimeColour: 0x%04X\n", config.localTimeColour);
  Serial.printf("  utcTimeColour: 0x%04X\n", config.utcTimeColour);
  
  // 🖼 Splash screen will be painted in runHamClock() just before the first
  // page draw — arming it here so the guard knows to do it.
  splashActive = true;
  
  // CREATE AND INITIALIZE ISS TRACKER
  issTracker = new ISSTracker();
  if (config.latitude != 0.0 && config.longitude != 0.0) {
    issTracker->setUserLocation(config.latitude, config.longitude);
    Serial.printf("🛰 ISS Tracker initialized for location: %.4f, %.4f\n", 
                  config.latitude, config.longitude);
  }
  
  // Determine initial mode - SIMPLIFIED!
  if (!wifiConfig.configured) {
    // No WiFi configured → Show QR code for initial setup
    Serial.println("No WiFi - starting SETUP mode");
    currentMode = MODE_SETUP;
    startSetupMode();
  } else {
    // WiFi is configured → Try to connect
    Serial.println("WiFi configured - connecting...");
        if (connectToWiFi()) {
      // Check if critical settings are complete
      bool hasApiKey = config.apiKeyValid;
      bool hasLocation = (config.latitude != 0.0 && config.longitude != 0.0);
      bool needsConfig = !hasApiKey || !hasLocation;
      
      if (needsConfig) {
        // Missing required settings → CONFIGURATION mode
        Serial.println("⚠ Configuration incomplete - entering CONFIG mode");
        Serial.printf("  API Key: %s\n", hasApiKey ? "✅ Set" : "❌ Missing");
        Serial.printf("  Location: %s\n", hasLocation ? "✅ Set" : "❌ Missing");
        
        currentMode = MODE_CONFIGURATION;
        startConfigurationMode();  // Shows config screen on display
      } else {
        // All required settings present → RUNNING mode
        Serial.println("✅ Fully configured - starting RUNNING mode");
        
        currentMode = MODE_RUNNING;
        startRunningMode();
      }
      } else {
      // WiFi connection failed → Back to SETUP mode
      Serial.println("WiFi failed - returning to SETUP mode");
      currentMode = MODE_SETUP;
      startSetupMode();
    }
  }
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  switch (currentMode) {
    case MODE_SETUP:
    case MODE_CONFIGURATION:
      server.handleClient();
      delay(10);  // Small delay for setup/config modes
      break;
      
    case MODE_RUNNING:
      runHamClock();
      // No delay here - let runHamClock control timing
      break;
  }
}

// ============================================================================
// CONFIGURATION MANAGEMENT - WiFi
// ============================================================================
void loadWiFiConfig() {
  prefs.begin("hamclock", false);
  wifiConfig.configured = prefs.getBool("wifi_configured", false);
  
  if (wifiConfig.configured) {
    prefs.getString("wifi_ssid", wifiConfig.ssid, sizeof(wifiConfig.ssid));
    prefs.getString("wifi_pass", wifiConfig.password, sizeof(wifiConfig.password));
    prefs.getString("tz_iso", wifiConfig.timezone_iso, sizeof(wifiConfig.timezone_iso));
    wifiConfig.timezone_offset = prefs.getLong("tz_offset", 0);
    
    Serial.printf("WiFi: %s, TZ: %s\n", wifiConfig.ssid, wifiConfig.timezone_iso);
  }
  prefs.end();
}

void saveWiFiConfig() {
  prefs.begin("hamclock", false);
  prefs.putString("wifi_ssid", wifiConfig.ssid);
  prefs.putString("wifi_pass", wifiConfig.password);
  prefs.putString("tz_iso", wifiConfig.timezone_iso);
  prefs.putLong("tz_offset", wifiConfig.timezone_offset);
  prefs.putBool("wifi_configured", true);
  prefs.end();
  
  Serial.println("✅ WiFi configuration saved");
}

// ============================================================================
// CONFIGURATION MANAGEMENT - HamClock (LittleFS + NVS)
// ============================================================================
void loadHamClockConfig() {
  // Load from LittleFS settings.json (primary)
  fs::File file = LittleFS.open("/settings.json", "r");
  if (file) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (!error) {
      config.latitude = doc["latitude"] | 0.0;
      config.longitude = doc["longitude"] | 0.0;
      config.localTimeColour = doc["localTimeColour"] | 0x07E0;
      config.localFrameColour = doc["localFrameColour"] | 0x07E0;
      config.utcTimeColour = doc["utcTimeColour"] | 0x07FF;
      config.utcFrameColour = doc["utcFrameColour"] | 0x07FF;
      config.bannerColour = doc["bannerColour"] | 0xF800; 
      config.italicClockFonts = doc["italicClockFonts"] | false;
      config.doubleFrame = doc["doubleFrame"] | true;
      config.bannerSpeed = doc["bannerSpeed"] | 23;
      config.bannerPixelsPerFrame = doc["bannerPixelsPerFrame"] | 1;
      config.screenSaverTimeout = doc["screenSaverTimeout"] | 7200000; //120 minutes
      config.autoPageChange = doc["autoPageChange"] | false;
      
      String logo = doc["startupLogo"] | "logo1.png";
      logo.toCharArray(config.startupLogo, sizeof(config.startupLogo));
      config.splashDuration = doc["splashDuration"] | 3000;
      
      String cs = doc["callsign"] | "";
      cs.toCharArray(config.callsign, sizeof(config.callsign));
      
      String gs = doc["gridSquare"] | "";
      gs.toCharArray(config.gridSquare, sizeof(config.gridSquare));
      
      Serial.println("✅ Settings loaded from LittleFS");
    }
  }
  
  // Load API key from NVS (kept separate for security)
  prefs.begin("hamclock", false);
  prefs.getString("apiKey", config.apiKey, sizeof(config.apiKey));
  config.apiKeyValid = prefs.getBool("apiKeyValid", false);
  config.utcOffsetSeconds = prefs.getInt("utcOffset", 0);

  // ADD THESE 4 LINES (NVS version, NOT JSON):
  prefs.getString("n2yoApiKey", config.n2yoApiKey, sizeof(config.n2yoApiKey));
  config.n2yoApiKeyValid = prefs.getBool("n2yoApiKeyValid", false);
  config.n2yoLatitude = prefs.getFloat("n2yoLatitude", 0.0);
  config.n2yoLongitude = prefs.getFloat("n2yoLongitude", 0.0);

  // ADD THESE DEBUG LINES:
  Serial.printf("🔍 N2YO Debug - Loaded from NVS:\n");
  Serial.printf("🔑 Key: '%s'\n", config.n2yoApiKey);
  Serial.printf("  Valid: %s\n", config.n2yoApiKeyValid ? "true" : "false");
  Serial.printf("  Lat: %.4f, Lon: %.4f\n", config.n2yoLatitude, config.n2yoLongitude);
  Serial.printf("  Key length: %d\n", strlen(config.n2yoApiKey));
  
  // Fallback to NVS if LittleFS didn't have location
  if (config.latitude == 0.0) {
    config.latitude = prefs.getFloat("latitude", 0.0);
    config.longitude = prefs.getFloat("longitude", 0.0);
  }
  prefs.end();
  
  Serial.printf("📋 Config: Lat=%.4f, Lon=%.4f, API=%s, Call=%s\n",
    config.latitude, config.longitude,
    config.apiKeyValid ? "✓" : "✗",
    strlen(config.callsign) > 0 ? config.callsign : "none");
}

void saveHamClockConfig() {
  // Save to LittleFS settings.json
  DynamicJsonDocument doc(2048);
  doc["latitude"] = config.latitude;
  doc["longitude"] = config.longitude;
  doc["apiKey"] = config.apiKey;
  doc["apiKeyValid"] = config.apiKeyValid;
  // Save N2YO API key
  doc["n2yoApiKey"] = config.n2yoApiKey;
  doc["n2yoApiKeyValid"] = config.n2yoApiKeyValid;
  doc["n2yoLatitude"] = config.n2yoLatitude;
  doc["n2yoLongitude"] = config.n2yoLongitude;
  doc["localTimeColour"] = config.localTimeColour;
  doc["localFrameColour"] = config.localFrameColour;
  doc["utcTimeColour"] = config.utcTimeColour;
  doc["utcFrameColour"] = config.utcFrameColour;
  doc["bannerColour"] = config.bannerColour;
  doc["italicClockFonts"] = config.italicClockFonts;
  doc["doubleFrame"] = config.doubleFrame;
  doc["bannerSpeed"] = config.bannerSpeed;
  doc["bannerPixelsPerFrame"] = config.bannerPixelsPerFrame;  // ADD THIS
  doc["screenSaverTimeout"] = config.screenSaverTimeout;
  doc["autoPageChange"] = config.autoPageChange;
  doc["startupLogo"] = config.startupLogo;
  doc["splashDuration"] = config.splashDuration;
  doc["callsign"] = config.callsign;
  doc["gridSquare"] = config.gridSquare;
  
  fs::File file = LittleFS.open("/settings.json", "w");
  if (file) {
    serializeJsonPretty(doc, file);
    file.close();
    Serial.println("✅ Saved to LittleFS");
  }
  
  // Save to NVS (backup + API key)
  prefs.begin("hamclock", false);
  prefs.putString("apiKey", config.apiKey);
  prefs.putBool("apiKeyValid", config.apiKeyValid);
  prefs.putFloat("latitude", config.latitude);
  prefs.putFloat("longitude", config.longitude);
  prefs.putInt("utcOffset", config.utcOffsetSeconds);
  prefs.putString("n2yoApiKey", config.n2yoApiKey);
  prefs.putBool("n2yoApiKeyValid", config.n2yoApiKeyValid);
  prefs.putFloat("n2yoLatitude", config.n2yoLatitude);
  prefs.putFloat("n2yoLongitude", config.n2yoLongitude);
  
  prefs.end();
  
  Serial.println("✅ Configuration saved");
}

// ============================================================================
// MODE: SETUP (QR Code WiFi Setup)
// ============================================================================
void startSetupMode() {
  String macAddr = WiFi.macAddress();
  macAddr.replace(":", "");
  String apName = String(AP_SSID_PREFIX) + macAddr.substring(6);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(apName.c_str(), AP_PASSWORD);
  
  Serial.printf("AP: %s @ %s\n", apName.c_str(), WiFi.softAPIP().toString().c_str());
  
  drawSetupScreen(apName);
  setupWebServerForSetup();
  server.begin();
}

void drawSetupScreen(String apName) {
  tft.fillScreen(TFT_BLACK);
  
  tft.setFreeFont(&DINPro_Regular15pt8b);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(50, 22);
  tft.print("HamClock Setup");
  
  tft.setFreeFont(&DINPro_Regular6pt8b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(50, 37);
  tft.print("Scan QR codes:");
  
  tft.setCursor(15, 60);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("1. WiFi");
  
  String wifiQR = "WIFI:T:nopass;S:" + apName + ";P:;H:false;;";
  drawQRCode(wifiQR, 20, 72, 4);   //WiFi QR code at position (20, 75) with scale = 3
  
  tft.setCursor(180, 60);
  tft.print("2. Config");
  
  drawQRCode("http://192.168.4.1", 185, 72, 4); //Config URL QR code at position (180, 75) with scale = 3
  
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(215, 210);
  tft.print("192.168.4.1");
}

void drawQRCode(String data, int x, int y, int scale) {
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, 0, data.c_str());
  
  int qrSize = qrcode.size * scale;
  int border = scale * 2;
  tft.fillRect(x - border, y - border, qrSize + (border * 2), qrSize + (border * 2), TFT_WHITE);
  
  for (uint8_t qy = 0; qy < qrcode.size; qy++) {
    for (uint8_t qx = 0; qx < qrcode.size; qx++) {
      uint16_t color = qrcode_getModule(&qrcode, qx, qy) ? TFT_BLACK : TFT_WHITE;
      tft.fillRect(x + (qx * scale), y + (qy * scale), scale, scale, color);
    }
  }
}

void setupWebServerForSetup() {
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, handleSaveWiFi);
  server.on("/reset", HTTP_GET, handleFactoryReset);
  server.onNotFound(handleNotFound);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  DynamicJsonDocument doc(1024);
  JsonArray networks = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 20; i++) {
    networks.add(WiFi.SSID(i));
  }
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveWiFi() {
  if (server.hasArg("ssid")) {
    server.arg("ssid").toCharArray(wifiConfig.ssid, sizeof(wifiConfig.ssid));
  }
  if (server.hasArg("password")) {
    server.arg("password").toCharArray(wifiConfig.password, sizeof(wifiConfig.password));
  }
  if (server.hasArg("time")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("time"));
    const char* iso = doc["iso"];
    if (iso) strncpy(wifiConfig.timezone_iso, iso, sizeof(wifiConfig.timezone_iso));
    wifiConfig.timezone_offset = doc["offset"] | 0;
  }
  
  wifiConfig.configured = true;
  saveWiFiConfig();
  
  server.send(200, "text/html", html_success);
  delay(2000);
  ESP.restart();
}

void handleFactoryReset() {
  prefs.begin("hamclock", false);
  prefs.clear();
  prefs.end();
  
  if (LittleFS.exists("/settings.json")) {
    LittleFS.remove("/settings.json");
  }
  
  server.send(200, "text/plain", "Reset complete. Rebooting...");
  delay(2000);
  ESP.restart();
}

// ============================================================================
// MODE: CONFIGURATION (LittleFS HTML Primary Interface)
// ============================================================================
void startConfigurationMode() {
  Serial.println("📋 CONFIGURATION mode - waiting for settings");

  // Display config message on screen
  tft.fillScreen(TFT_BLACK);

  tft.setFreeFont(&DINPro_Regular11pt7b);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 20);
  tft.print("Local");

  tft.setCursor(10,40);
  tft.println("Configuration Mode");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 80);
  tft.println("Open your browser:");

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10, 110);
  tft.print("http://");
  tft.print(WiFi.localIP().toString());
  tft.println("/settings");
  
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 150);
  tft.println("Required Settings:");
  tft.setCursor(10, 175);
  tft.println("1. Weather API Key");
  tft.setCursor(10, 200);
  tft.println("2. Latitude & Longitude");
  
  // Setup web server for configuration
  setupWebServerForConfiguration();
  server.begin();

  //reset font to internal
  tft.setFreeFont(nullptr);
  
  Serial.printf("📡 Config available at: http://%s/settings\n", WiFi.localIP().toString().c_str());
  Serial.println("📋 CONFIGURATION mode - LittleFS HTML active");
  Serial.println("Access: http://" + WiFi.localIP().toString());
}

void setupWebServerForConfiguration() {
  // Serve LittleFS HTML files (primary configuration interface)
  // IMPORTANT: Route / to settings.html during configuration
  server.on("/", HTTP_GET, []() { 
    serveFile("/settings.html", "text/html");  // Changed from index.html
  });
  server.on("/settings", HTTP_GET, []() { 
    serveFile("/settings.html", "text/html"); 
  });

  server.on("/apikey.html", HTTP_GET, []() { serveFile("/apikey.html", "text/html"); });
  server.on("/config.html", HTTP_GET, []() { serveFile("/config.html", "text/html"); });
  
  // Serve static assets (CSS, JS, fonts, images) - needed by HTML pages
  server.on("/style.css", HTTP_GET, []() { serveFile("/style.css", "text/css"); });
  server.on("/script.js", HTTP_GET, []() { serveFile("/script.js", "application/javascript"); });
  server.on("/favicon.ico", HTTP_GET, []() { serveFile("/favicon.ico", "image/x-icon"); });
  server.on("/github.png", HTTP_GET, []() { serveFile("/github.png", "image/png"); });
  server.on("/logo1.png", HTTP_GET, []() { serveFile("/logo1.png", "image/png"); });
  server.on("/logo2.png", HTTP_GET, []() { serveFile("/logo2.png", "image/png"); });
  server.on("/logo3.png", HTTP_GET, []() { serveFile("/logo3.png", "image/png"); });
  server.on("/logo4.png", HTTP_GET, []() { serveFile("/logo4.png", "image/png"); });
  server.on("/fonts/digital.ttf", HTTP_GET, []() { serveFile("/fonts/digital.ttf", "font/ttf"); });
  server.on("/fonts/digital7monoitalic.ttf", HTTP_GET, []() { serveFile("/fonts/digital7monoitalic.ttf", "font/ttf"); });
  server.on("/fonts/digitalitalic.ttf", HTTP_GET, []() { serveFile("/fonts/digitalitalic.ttf", "font/ttf"); });
  server.on("/digital.ttf", HTTP_GET, []() { serveFile("/digital.ttf", "font/ttf"); });
  server.on("/digital7monoitalic.ttf", HTTP_GET, []() { serveFile("/digital7monoitalic.ttf", "font/ttf"); });
  server.on("/digitalitalic.ttf", HTTP_GET, []() { serveFile("/digitalitalic.ttf", "font/ttf"); });
  
  // Configuration API endpoints (used by LittleFS HTML)
  server.on("/config", HTTP_GET, handleGetConfig);
  server.on("/getApiKey", HTTP_GET, handleGetApiKey);
  server.on("/saveApiKey", HTTP_GET, handleSaveApiKey);
  // ADD THESE TWO LINES:
  server.on("/getN2YOApiKey", HTTP_GET, handleGetN2YOApiKey);
  server.on("/saveN2YOApiKey", HTTP_GET, handleSaveN2YOApiKey);
    
  server.on("/setcolor", HTTP_POST, handleSetColor);
  server.on("/setspeed", HTTP_POST, handleSetSpeed);
  server.on("/setpixelspeed", HTTP_POST, handleSetPixelSpeed);  // ADD THIS
  server.on("/setposition", HTTP_POST, handleSetPosition);
  server.on("/setbootimage", HTTP_POST, handleSetBootImage);
  server.on("/uploadpng", HTTP_POST, []() { /* multipart handled by callback */ }, handlePNGUpload);
  server.on("/setScreenSaverTime", HTTP_POST, handleSetScreenSaver);
  server.on("/setAutoPage", HTTP_GET, handleSetAutoPage);
  server.on("/setitalic", HTTP_POST, handleSetItalic);
  server.on("/scrolltext", HTTP_GET, handleScrollText);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/saveall", HTTP_POST, handleSaveAll);
  server.on("/setcallsign", HTTP_POST, handleSetCallsign);
  
  server.onNotFound(handleNotFound);
}

void serveFile(const char* path, const char* contentType) {
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
  } else {
    server.send(404, "text/plain", "File not found");
  }
}

// Configuration API Handlers
void handleGetConfig() {
  DynamicJsonDocument doc(2048);
  doc["APIkeyIsValid"] = config.apiKeyValid;
  doc["latitude"] = config.latitude;
  doc["longitude"] = config.longitude;
  doc["localTimeColour"] = config.localTimeColour;
  doc["localFrameColour"] = config.localFrameColour;
  doc["utcTimeColour"] = config.utcTimeColour;
  doc["utcFrameColour"] = config.utcFrameColour;
  doc["bannerColour"] = config.bannerColour;
  doc["italicClockFonts"] = config.italicClockFonts;
  doc["doubleFrame"] = config.doubleFrame;
  doc["bannerSpeed"] = config.bannerSpeed;
  doc["bannerPixelsPerFrame"] = config.bannerPixelsPerFrame;
  doc["autoPageChange"] = config.autoPageChange;
  doc["startupLogo"] = config.startupLogo;
  doc["splashDuration"] = config.splashDuration;
  doc["callsign"] = config.callsign;
  doc["gridSquare"] = config.gridSquare;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetApiKey() {
  server.send(200, "text/plain", config.apiKey);
}
void handleSaveApiKey() {
  bool hasKey = server.hasArg("key");
  bool hasLat = server.hasArg("latitude");
  bool hasLon = server.hasArg("longitude");
  
  if (hasKey && hasLat && hasLon) {
    // Save API key
    String key = server.arg("key");
    key.toCharArray(config.apiKey, sizeof(config.apiKey));
    config.apiKeyValid = (key.length() > 0);
    
    // Save location
    config.latitude = server.arg("latitude").toFloat();
    config.longitude = server.arg("longitude").toFloat();
    
    saveHamClockConfig();
    
    Serial.printf("✅ Required settings saved:\n");
    Serial.printf("🔑 API Key: %s\n", config.apiKeyValid ? "Set" : "Not set");
    Serial.printf("📍 Location: %.4f, %.4f\n", config.latitude, config.longitude);
    
    // Check if all required fields are complete
    bool fullyConfigured = config.apiKeyValid && 
                          config.latitude != 0.0 && 
                          config.longitude != 0.0;
    
    if (fullyConfigured) {
      // All required settings complete - reboot!
      server.send(200, "text/html", 
        "<html><body style='font-family:Arial;text-align:center;padding:50px;background:#0f172a;color:#fff;'>"
        "<h2 style='color:#10b981;'>✅ Configuration Complete!</h2>"
        "<p>Starting HamClock in 3 seconds...</p>"
        "<script>setTimeout(function(){ window.location.href='/'; }, 3000);</script>"
        "</body></html>");
      
      Serial.println("🚀 All required settings complete - rebooting to RUNNING mode");
      delay(2000);
      ESP.restart();
    } else {
      // Shouldn't happen, but handle gracefully
      server.send(200, "text/plain", "Settings saved");
    }
  } else {
    server.send(400, "text/plain", "Missing required parameters (key, latitude, longitude)");
  }
}

void handleGetN2YOApiKey() {
  server.send(200, "text/plain", config.n2yoApiKey);
}

void handleSaveN2YOApiKey() {
  if (server.hasArg("key")) {
    String key = server.arg("key");
    
    // Save to NVS
    prefs.begin("hamclock", false);
    prefs.putString("n2yoApiKey", key);
    prefs.putBool("n2yoApiKeyValid", key.length() > 0);
    
    // Optional custom location
    if (server.hasArg("latitude") && server.hasArg("longitude")) {
      float lat = server.arg("latitude").toFloat();
      float lon = server.arg("longitude").toFloat();
      if (lat != 0.0 && lon != 0.0) {
        prefs.putFloat("n2yoLatitude", lat);
        prefs.putFloat("n2yoLongitude", lon);
        Serial.printf("✅ N2YO custom location: %.4f, %.4f\n", lat, lon);
      }
    }
    
    prefs.end();
    
    Serial.println("✅ N2YO API key saved - rebooting to fetch passes");
    
    // Send response
    server.send(200, "text/plain", "API key saved");
    
    // Reboot to fetch passes with new key
    delay(1000);
    ESP.restart();
    
  } else {
    server.send(400, "text/plain", "Missing API key parameter");
  }
}
    
void handleISSPassesAPI() {
  // Simple JSON response with just the next pass
  String response = "{";
  response.reserve(512);  // ADD THIS - pre-allocate memory
  
  
  // Check if N2YO API key is configured
  if (!config.n2yoApiKeyValid || strlen(config.n2yoApiKey) == 0) {
    response += "\"hasApiKey\":false";
    response += "}";
    server.send(200, "application/json", response);
    return;
  }
  
  response += "\"hasApiKey\":true,";
  
  // Check if we have pass data
  if (!issTracker || !issTracker->hasPassData()) {
    response += "\"hasData\":false";
    response += "}";
    server.send(200, "application/json", response);
    return;
  }
  
  response += "\"hasData\":true,";
  
  // Location
  float lat = (config.n2yoLatitude != 0.0) ? config.n2yoLatitude : config.latitude;
  float lon = (config.n2yoLongitude != 0.0) ? config.n2yoLongitude : config.longitude;
  response += "\"latitude\":" + String(lat, 4) + ",";
  response += "\"longitude\":" + String(lon, 4) + ",";

// Try this again now that we have more space:
time_t now;
time(&now);
unsigned long lastUpdate = now;
response += "\"lastUpdate\":";
response += String(lastUpdate);
response += ",";
  
int passCount = issTracker->getPassCount();

response += "\"passes\":[";

for (int i = 0; i < passCount && i < 3; i++) {
  ISSPass pass = issTracker->getPass(i);
  if (!pass.isValid) continue;
  
  if (i > 0) response += ",";  // Add comma between passes
  
  response += "{";
  response += "\"startUTC\":" + String(pass.riseTime) + ",";
  response += "\"maxUTC\":" + String(pass.maxTime) + ",";
  response += "\"endUTC\":" + String(pass.setTime) + ",";
  response += "\"maxEl\":" + String(pass.maxElevation, 1) + ",";
  response += "\"mag\":" + String(pass.magnitude, 1) + ",";
  response += "\"startAzCompass\":\"" + String(pass.direction) + "\",";
  response += "\"endAzCompass\":\"" + String(pass.endDirection) + "\",";
  response += "\"duration\":" + String(pass.setTime - pass.riseTime);
  response += "}";
}

response += "]}";
  
  server.send(200, "application/json", response);
}

void handleDashboardAPI() {
  String response = "{";
  
  // Time info
  time_t now;
  time(&now);
  struct tm* timeInfo = localtime(&now);
  char localTimeStr[32];
  strftime(localTimeStr, sizeof(localTimeStr), "%I:%M:%S %p", timeInfo);
  
  timeInfo = gmtime(&now);
  char utcTimeStr[32];
  strftime(utcTimeStr, sizeof(utcTimeStr), "%H:%M:%S UTC", timeInfo);
  
  response += "\"time\":{";
  response += "\"local\":\"" + String(localTimeStr) + "\",";
  response += "\"utc\":\"" + String(utcTimeStr) + "\"";
  response += "},";
  
  // Quick stats
  response += "\"stats\":{";
  
  // ISS
  if (issTracker && issTracker->isDataValid()) {
    response += "\"issLat\":" + String(issTracker->getLatitude(), 2) + ",";
    response += "\"issLon\":" + String(issTracker->getLongitude(), 2) + ",";
  }
  
  // Solar - FIXED
  if (solarData && solarData->isDataValid()) {
    SolarIndices solar = solarData->getIndices();  // Get the struct
    response += "\"sfi\":" + String(solar.sfi) + ",";
    response += "\"kIndex\":" + String(solar.kIndex) + ",";
  }
  
  // Weather - FIXED
  if (weatherData && weatherData->isDataValid()) {
    WeatherInfo weather = weatherData->getWeather();  // Get the struct
    response += "\"temp\":" + String(weather.tempF, 1) + ",";
    response += "\"conditions\":\"" + weather.description + "\",";
  }
  
  // PSK
  if (pskReporter && pskReporter->isDataValid()) {
    int spotCount = 0;
    pskReporter->getSpots(spotCount);
    response += "\"pskSpots\":" + String(spotCount) + ",";
  }
  
  response += "\"callsign\":\"" + String(config.callsign) + "\"";
  response += "}";
  
  response += "}";
  
  server.send(200, "application/json", response);
}

void handleStatusAPI() {
  String response = "{";
  
  response += "\"wifi\":{";
  response += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  response += "\"ssid\":\"" + String(wifiConfig.ssid) + "\",";
  response += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  response += "\"rssi\":" + String(WiFi.RSSI());
  response += "},";
  
  response += "\"iss\":{";
  if (issTracker && issTracker->isDataValid()) {
    response += "\"valid\":true,";
    response += "\"latitude\":" + String(issTracker->getLatitude(), 4) + ",";
    response += "\"longitude\":" + String(issTracker->getLongitude(), 4);
  } else {
    response += "\"valid\":false";
  }
  response += "},";
  
  response += "\"solar\":{";
  response += "\"valid\":" + String(solarData && solarData->isDataValid() ? "true" : "false");
  response += "},";
  
  response += "\"weather\":{";
  response += "\"valid\":" + String(weatherData && weatherData->isDataValid() ? "true" : "false");
  response += "},";
  
  response += "\"psk\":{";
  response += "\"valid\":" + String(pskReporter && pskReporter->isDataValid() ? "true" : "false");
  response += "},";
  
  // System info
  response += "\"system\":{";
  response += "\"uptime\":" + String(millis() / 1000) + ",";
  response += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  response += "\"heapSize\":" + String(ESP.getHeapSize());
  response += "}";
  
  response += "}";
  
  server.send(200, "application/json", response);
}

void handleSolarAPI() {
  String response = "{";
  
  if (!solarData || !solarData->isDataValid()) {
    response += "\"valid\":false";
    response += "}";
    server.send(200, "application/json", response);
    return;
  }
  
  response += "\"valid\":true,";
  
  // Get solar data struct - FIXED
  SolarIndices solar = solarData->getIndices();
  
  response += "\"sfi\":" + String(solar.sfi) + ",";
  response += "\"ssn\":" + String(solar.ssn) + ",";
  response += "\"aIndex\":" + String(solar.aIndex) + ",";
  response += "\"kIndex\":" + String(solar.kIndex) + ",";
  response += "\"xray\":\"" + solar.xRay + "\",";
  response += "\"bz\":" + String(solar.bz, 1) + ",";
  response += "\"timestamp\":" + String(millis() / 1000);
  response += "}";
  
  server.send(200, "application/json", response);
}

void handlePropagationAPI() {
  String response = "{";
  response.reserve(8192);  // Pre-allocate for large response
  
  // Check if PSK data is valid
  if (!pskReporter || !pskReporter->isDataValid()) {
    response += "\"valid\":false";
    response += "}";
    server.send(200, "application/json", response);
    return;
  }
  
  response += "\"valid\":true,";
  
  // Get spot data
  int spotCount = 0;
  PSKSpot* spots = pskReporter->getSpots(spotCount);
  
  response += "\"spotCount\":" + String(spotCount) + ",";
  response += "\"mostActiveBand\":\"" + pskReporter->getMostActiveBand() + "\",";
  response += "\"callsign\":\"" + String(config.callsign) + "\",";
  
  // Determine how many spots to show
  int maxSpots = (strlen(config.callsign) > 0) ? spotCount : min(50, spotCount);
  
  response += "\"spots\":[";
  
  for (int i = 0; i < maxSpots; i++) {
    PSKSpot spot = spots[i];
    
    if (i > 0) response += ",";
    
    // Convert timestamp to readable time
    time_t spotTime = spot.timestamp;
    struct tm* timeInfo = gmtime(&spotTime);
    char timeStr[16];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeInfo);
    
    // Convert frequency to MHz
    float freqMHz = spot.frequency / 1000000.0;
    
    response += "{";
    response += "\"time\":\"" + String(timeStr) + "\",";
    response += "\"txCall\":\"" + String(spot.txCallsign) + "\",";
    response += "\"rxCall\":\"" + String(spot.rxCallsign) + "\",";
    response += "\"band\":\"" + String(spot.band) + "\",";
    response += "\"freqMHz\":\"" + String(freqMHz, 3) + "\",";
    response += "\"mode\":\"" + String(spot.mode) + "\",";
    response += "\"snr\":\"" + String(spot.snr) + "\",";
    response += "\"distance\":\"" + String((int)spot.distance) + "\"";
    response += "}";
  }
  
  response += "]}";
  
  server.send(200, "application/json", response);
}

void handleSetCallsign() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    String cs = doc["callsign"] | "";
    String gs = doc["gridSquare"] | "";
    cs.toCharArray(config.callsign, sizeof(config.callsign));
    gs.toCharArray(config.gridSquare, sizeof(config.gridSquare));
    saveHamClockConfig();
    server.send(200, "text/plain", "Callsign saved");
    Serial.printf("✅ Callsign: %s, Grid: %s\n", config.callsign, config.gridSquare);
  }
}

void handleSetColor() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    String target = doc["target"];
    
    if (target == "doubleFrame") {
      config.doubleFrame = !doc["value"].as<bool>();
    } else {
      uint16_t color = doc["color"];
      if (target == "localTimeDigits") config.localTimeColour = color;
      else if (target == "localTimeFrame") config.localFrameColour = color;
      else if (target == "utcTimeDigits") config.utcTimeColour = color;
      else if (target == "utcTimeFrame") config.utcFrameColour = color;
      else if (target == "weatherBannerText") config.bannerColour = color;
    }
    saveHamClockConfig();
    reloadDisplayConfig();  // Apply immediately!
    
    server.send(200, "text/plain", "Color updated!");
    // Remove ESP.restart()
  }
}

void handleSetSpeed() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    
    int sliderValue = doc["speed"];  // 0-45 range
    
    // Always use 1 pixel per frame for smoothness (like original Hosyond)
    config.bannerPixelsPerFrame = 1;
    
    // Map slider to frame delay (ms): left=slow, right=fast
    // HTML range slider: LEFT outputs 0, RIGHT outputs 45
    // So: 0 (left) → 80ms (slow), 45 (right) → 10ms (fast)
    config.bannerSpeed = map(sliderValue, 0, 45, 80, 10);
    
    Serial.printf("Slider=%d -> Speed=%dms (1px/frame)\n", sliderValue, config.bannerSpeed);
    
    saveHamClockConfig();
    reloadDisplayConfig();
    reloadDisplayConfig();
    server.send(200, "text/plain", "Speed updated!");
  }
}

void handleSetPosition() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    config.latitude = doc["latitude"];
    config.longitude = doc["longitude"];
    saveHamClockConfig();
    
    Serial.printf("✅ Position updated: %.6f, %.6f\n", config.latitude, config.longitude);
    
    // Just save, no reboot (optional setting change)
    server.send(200, "text/plain", "Position updated");
  }
}

void handleSetBootImage() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    String logo = doc["bootImageId"];
    logo.toCharArray(config.startupLogo, sizeof(config.startupLogo));
    saveHamClockConfig();
    server.send(200, "text/plain", "Boot image set");
    delay(500);
    ESP.restart();
  }
}

void handleSetScreenSaver() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    config.screenSaverTimeout = doc["screenSaverTimeout"];
    saveHamClockConfig();
    server.send(200, "text/plain", "Screen saver set");
  }
}

void handleSetAutoPage() {
  if (server.hasArg("enabled")) {
    config.autoPageChange = (server.arg("enabled") == "true");
    saveHamClockConfig();
    server.send(200, "text/plain", "Auto page set");
  }
}

void handleSetItalic() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    config.italicClockFonts = doc["italicClockFonts"];
    saveHamClockConfig();
    server.send(200, "text/plain", "Saved! Restarting in 2 seconds...");
    delay(2000);
    ESP.restart();  // ADD THIS
  }
}


void handleSetPixelSpeed() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    config.bannerPixelsPerFrame = doc["pixelSpeed"];
    
    // Clamp to 1-10
    if (config.bannerPixelsPerFrame < 1) config.bannerPixelsPerFrame = 1;
    if (config.bannerPixelsPerFrame > 10) config.bannerPixelsPerFrame = 10;
    
    saveHamClockConfig();
    reloadDisplayConfig();
    server.send(200, "text/plain", "Pixel speed updated!");
  }
}


void handleScrollText() {
  // Return the web-safe banner text built by DisplayManager each frame.
  // Falls back to a placeholder if weather data hasn't arrived yet.
  if (display) {
    String text = display->getWebBannerText();
    if (text.length() > 0) {
      server.send(200, "text/plain", text);
      return;
    }
  }
  // Fallback before weather data is available
  String fallback = "HamClock";
  if (strlen(config.callsign) > 0) fallback += " - " + String(config.callsign);
  fallback += "    Weather data loading...";
  server.send(200, "text/plain", fallback);
}

// ============================================================================
// PNG DECODER - LittleFS file callbacks (required by PNGdec library)
// ============================================================================
void *pngFileOpen(const char *filename, int32_t *size) {
  String path = "/" + String(filename);
  pngFile = LittleFS.open(path, "r");
  if (!pngFile) {
    Serial.printf("❌ PNG open failed: %s\n", path.c_str());
    return nullptr;
  }
  *size = pngFile.size();
  return (void *)&pngFile;
}

void pngFileClose(void *handle) {
  ((File *)handle)->close();
}

int32_t pngFileRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
  return ((File *)handle->fHandle)->read(buffer, length);
}

int32_t pngFileSeek(PNGFILE *handle, int32_t position) {
  return ((File *)handle->fHandle)->seek(position);
}

// Display a PNG from LittleFS on the TFT. duration_ms=0 means no delay after.
void displayPNGfromLittleFS(const char *filename, int duration_ms) {
  int16_t rc = png.open(filename, pngFileOpen, pngFileClose, pngFileRead, pngFileSeek,
    [](PNGDRAW *pDraw) {
      uint16_t lineBuffer[320];
      png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xFFFFFFFF);
      tft.pushImage(0, pDraw->y, pDraw->iWidth, 1, lineBuffer);
    });

  if (rc == PNG_SUCCESS) {
    Serial.printf("🖼  Displaying splash: %s\n", filename);
    tft.startWrite();
    png.decode(nullptr, 0);
    tft.endWrite();
  } else {
    Serial.printf("❌ PNG decode failed for: %s (rc=%d)\n", filename, rc);
  }

  if (duration_ms > 0) delay(duration_ms);
}

// ============================================================================
// PNG UPLOAD HANDLER (multipart form upload from browser)
// ============================================================================
void handlePNGUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("📁 Uploading PNG: %s\n", upload.filename.c_str());

    // Show status on TFT
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont(&DINPro_Regular15pt8b);
    tft.drawCentreString("Receiving", 160, 30, 1);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawCentreString("New Splash Screen", 160, 80, 1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setFreeFont(&DINPro_Regular6pt8b);
    tft.drawCentreString("Please wait...", 160, 140, 1);

    // Open logo4.png for writing (uploaded custom image overwrites slot 4)
    File file = LittleFS.open("/logo4.png", FILE_WRITE);
    if (!file) {
      Serial.println("❌ Failed to open /logo4.png for writing");
    } else {
      file.close();
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    File file = LittleFS.open("/logo4.png", FILE_APPEND);
    if (file) {
      file.write(upload.buf, upload.currentSize);
      file.close();
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    Serial.printf("✅ Upload complete: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);
    server.send(200, "text/plain", "Upload complete");

    // Set as current splash and preview it
    strcpy(config.startupLogo, "logo4.png");
    saveHamClockConfig();
    displayPNGfromLittleFS("logo4.png", 3000);

    // Restore normal display
    tft.fillScreen(TFT_BLACK);
    if (display) {
      display->begin();
    }
  }
}

void handlePing() {
  server.send(200, "text/plain", "pong");
}

void handleSaveAll() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, server.arg("plain"));
    
    config.latitude = doc["latitude"] | config.latitude;
    config.longitude = doc["longitude"] | config.longitude;
    config.italicClockFonts = doc["italicClockFonts"] | config.italicClockFonts;
    config.doubleFrame = doc["doubleFrame"] | config.doubleFrame;
    config.bannerSpeed = doc["bannerSpeed"] | config.bannerSpeed;
    config.screenSaverTimeout = doc["screenSaverTimeout"] | config.screenSaverTimeout;
    
    saveHamClockConfig();
    server.send(200, "text/plain", "Settings saved - Starting HamClock...");
    
    Serial.println("✅ All settings saved - transitioning to RUNNING mode");
    delay(1000);
    
    // Check if we can transition to RUNNING mode
    // Only API key and location required, callsign optional
    if (config.apiKeyValid && config.latitude != 0.0 && config.longitude != 0.0) {
      currentMode = MODE_RUNNING;
      server.stop();
      startRunningMode();
    } else {
      Serial.println("⚠ Still missing configuration, staying in CONFIGURATION mode");
      Serial.printf("  Need: API Key=%s, Lat/Lon=(%.4f, %.4f)\n", 
        config.apiKeyValid ? "✓" : "✗", config.latitude, config.longitude);
      ESP.restart();
    }
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
}

// ============================================================================
// MODE: RUNNING (HamClock Application with Config Link)
// ============================================================================
void startRunningMode() {
  Serial.println("🚀 Starting HamClock RUNNING mode...");
  
  // Initialize NTP for time sync
  configTzTime("CST6CDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for NTP time sync...");
  
  // Wait up to 5 seconds for time sync
  int attempts = 0;
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (getLocalTime(&timeinfo)) {
    Serial.println("\n✅ Time synchronized");
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.printf("Current time: %s UTC\n", timeStr);
  } else {
    Serial.println("\n⚠ Time sync failed, continuing anyway");
  }
  
  // Initialize components with loaded configuration
  issTracker = new ISSTracker();
  issTracker->setUserLocation(config.latitude, config.longitude);
  solarData = new SolarData();
  
  // Set global weather coordinates (declared in weather_data.cpp)
  extern float g_weatherLat;
  extern float g_weatherLon;
  g_weatherLat = config.latitude;
  g_weatherLon = config.longitude;
  
  // Create WeatherData (will use g_weatherLat/g_weatherLon)
  weatherData = new WeatherData(config.apiKey, "dummy", "US");
  
  pskReporter = new PSKReporter(strlen(config.callsign) > 0 ? config.callsign : "");
  display = new DisplayManager(&tft);

  display->begin();

  // ADD THE CONFIG HERE:
  DisplayConfig displayConfig;
  displayConfig.doubleFrame = config.doubleFrame;
  displayConfig.italicClockFonts = config.italicClockFonts;
  displayConfig.localFrameColour = config.localFrameColour;
  displayConfig.utcFrameColour = config.utcFrameColour;
  displayConfig.localTimeColour = config.localTimeColour;
  displayConfig.utcTimeColour = config.utcTimeColour;
  displayConfig.bannerSpeed = config.bannerSpeed;
  displayConfig.bannerPixelsPerFrame = config.bannerPixelsPerFrame;
  displayConfig.bannerColour = config.bannerColour;

  display->setConfig(displayConfig);

  Serial.printf("🎨 Display configured with colors:\n");
  Serial.printf("  Local: 0x%04X / 0x%04X\n", displayConfig.localTimeColour, displayConfig.localFrameColour);
  Serial.printf("  UTC: 0x%04X / 0x%04X\n", displayConfig.utcTimeColour, displayConfig.utcFrameColour);

  touch = new TouchHandler(&tft);
  touch->begin();
  
  // Setup integrated web server (HamClock status + link to config)
  setupWebServerForRunning();
  server.begin();
  
  Serial.println("✅ HamClock RUNNING mode active");
  Serial.println("📡 Config available at: http://" + WiFi.localIP().toString() + "/settings");
  
  // 🚀 Start background task for component HTTP fetches — keeps main loop responsive
  xTaskCreatePinnedToCore(componentUpdateWorker, "CompUpdate", 8192, NULL, 1, NULL, 1);
  Serial.println("✅ Background component update task started");
  
  // 🖼 Repaint splash now that all init is done. connectToWiFi() and display->begin()
  // both cleared the screen during startup. This puts the splash back on so the
  // splashActive guard in runHamClock() can hold it for config.splashDuration.
  if (splashActive) {
    displayPNGfromLittleFS(config.startupLogo, 0);
    splashDisplayedAt = millis();  // Reset timer — duration starts NOW
  }
}


  // Add this NEW function here (before the handlers)
void reloadDisplayConfig() {
 Serial.println("🔄 reloadDisplayConfig() called!");
  if (display) {
  Serial.println("✅ display pointer valid"); 
    DisplayConfig displayConfig;
    displayConfig.doubleFrame = config.doubleFrame;
    displayConfig.italicClockFonts = config.italicClockFonts;
    displayConfig.localFrameColour = config.localFrameColour;
    displayConfig.utcFrameColour = config.utcFrameColour;
    displayConfig.localTimeColour = config.localTimeColour;
    displayConfig.utcTimeColour = config.utcTimeColour;
    displayConfig.bannerSpeed = config.bannerSpeed;
    displayConfig.bannerPixelsPerFrame = config.bannerPixelsPerFrame;
    displayConfig.bannerColour = config.bannerColour;
    
    display->setConfig(displayConfig);
    Serial.printf("🔄 Display config reloaded: bannerSpeed=%d\n", config.bannerSpeed);
  } else {
    Serial.println("❌ display pointer is NULL!");  // ADD THIS
  }
}

void setupWebServerForRunning() {
  // IMPORTANT: Register running mode pages FIRST (they take priority)
  // ========== NEW: LittleFS-BASED PAGES ==========
  server.on("/", HTTP_GET, []() { 
    serveFile("/dashboard.html", "text/html");  // Main dashboard
  });

  server.on("/settings", HTTP_GET, []() { 
    serveFile("/settings.html", "text/html"); 
  });

  // ADD THESE - Configuration pages accessible from /settings
  server.on("/apikey.html", HTTP_GET, []() { 
    serveFile("/apikey.html", "text/html"); 
  });
  
  server.on("/settings/apikey", HTTP_GET, []() { 
    serveFile("/apikey.html", "text/html"); 
  });

  server.on("/status", HTTP_GET, []() { 
    serveFile("/status.html", "text/html"); 
  });

  server.on("/solar", HTTP_GET, []() { 
    serveFile("/solar.html", "text/html"); 
  });
  
  server.on("/propagation", HTTP_GET, []() { 
    serveFile("/propagation.html", "text/html"); 
  });
  
  server.on("/iss-passes", HTTP_GET, []() { 
    serveFile("/iss-passes.html", "text/html"); 
  });
  
  // API Endpoints (provide JSON data)
  server.on("/api/dashboard", HTTP_GET, handleDashboardAPI);
  server.on("/api/status", HTTP_GET, handleStatusAPI);
  server.on("/api/solar", HTTP_GET, handleSolarAPI);
  server.on("/api/propagation", HTTP_GET, handlePropagationAPI);
  server.on("/api/iss-passes", HTTP_GET, handleISSPassesAPI);
 
  // Keep configuration API endpoints active for settings page
  server.on("/config", HTTP_GET, handleGetConfig);
  server.on("/getApiKey", HTTP_GET, handleGetApiKey);
  server.on("/saveApiKey", HTTP_GET, handleSaveApiKey);
  // ADD THESE TWO LINES:
  server.on("/getN2YOApiKey", HTTP_GET, handleGetN2YOApiKey);
  server.on("/saveN2YOApiKey", HTTP_GET, handleSaveN2YOApiKey);
  
  server.on("/setcolor", HTTP_POST, handleSetColor);
  server.on("/setspeed", HTTP_POST, handleSetSpeed);
  server.on("/setpixelspeed", HTTP_POST, handleSetPixelSpeed);  // ADD THIS
  server.on("/setposition", HTTP_POST, handleSetPosition);
  server.on("/setbootimage", HTTP_POST, handleSetBootImage);
  server.on("/uploadpng", HTTP_POST, []() { /* multipart handled by callback */ }, handlePNGUpload);
  server.on("/setScreenSaverTime", HTTP_POST, handleSetScreenSaver);
  server.on("/setAutoPage", HTTP_GET, handleSetAutoPage);
  server.on("/setitalic", HTTP_POST, handleSetItalic);
  server.on("/scrolltext", HTTP_GET, handleScrollText);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/saveall", HTTP_POST, handleSaveAll);
  server.on("/setcallsign", HTTP_POST, handleSetCallsign);

  // Serve LittleFS assets at ORIGINAL paths (for HTML to find them)
  server.on("/digital.ttf", HTTP_GET, []() { serveFile("/digital.ttf", "font/ttf"); });
  server.on("/digitalitalic.ttf", HTTP_GET, []() { serveFile("/digitalitalic.ttf", "font/ttf"); });
  server.on("/digital7monoitalic.ttf", HTTP_GET, []() { serveFile("/digital7monoitalic.ttf", "font/ttf"); });
  server.on("/logo1.png", HTTP_GET, []() { serveFile("/logo1.png", "image/png"); });
  server.on("/logo2.png", HTTP_GET, []() { serveFile("/logo2.png", "image/png"); });
  server.on("/logo3.png", HTTP_GET, []() { serveFile("/logo3.png", "image/png"); });
  server.on("/logo4.png", HTTP_GET, []() { serveFile("/logo4.png", "image/png"); });
  server.on("/github.png", HTTP_GET, []() { serveFile("/github.png", "image/png"); });
  server.on("/favicon.ico", HTTP_GET, []() { serveFile("/favicon.ico", "image/x-icon"); });
  server.on("/style.css", HTTP_GET, []() { serveFile("/style.css", "text/css"); });
  server.on("/script.js", HTTP_GET, []() { serveFile("/script.js", "application/javascript"); });
  
  // Keep /assets/ versions too if you want
  // Serve LittleFS assets under /assets path to avoid conflicts
  server.on("/assets/style.css", HTTP_GET, []() { serveFile("/style.css", "text/css"); });
  server.on("/assets/script.js", HTTP_GET, []() { serveFile("/script.js", "application/javascript"); });
  server.on("/assets/logo1.png", HTTP_GET, []() { serveFile("/logo1.png", "image/png"); });
  server.on("/assets/logo2.png", HTTP_GET, []() { serveFile("/logo2.png", "image/png"); });
  server.on("/assets/logo3.png", HTTP_GET, []() { serveFile("/logo3.png", "image/png"); });
  server.on("/assets/logo4.png", HTTP_GET, []() { serveFile("/logo4.png", "image/png"); });
  
  server.onNotFound(handleNotFound);
}

// ============================================================================
// BACKGROUND COMPONENT UPDATE WORKER (FreeRTOS task)
// Runs blocking HTTP fetches off the main loop so display stays smooth.
// ============================================================================
void componentUpdateWorker(void*) {
  while (true) {
    if (pendingComponentUpdate >= 0) {
      int comp = pendingComponentUpdate;
      pendingComponentUpdate = -1;  // Clear flag before the call so main loop can queue next
      
      unsigned long t0 = millis();
      switch (comp) {
        case 0: if (issTracker)  issTracker->update(true);  break;
        case 1: if (solarData)   solarData->update(true);   break;
        case 2: if (weatherData) weatherData->update(true); break;
        case 3: if (pskReporter) pskReporter->update(true); break;
      }
      unsigned long t1 = millis() - t0;
      if (t1 > 100) {  // Only log slow fetches >100ms
        Serial.printf("🔧 BG: Component %d took %lu ms\n", comp, t1);
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);  // 10ms poll interval
  }
}

void runHamClock() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastWebCheck = 0;
  static unsigned long lastDisplayUpdate = 0;
  static unsigned long lastTouchCheck = 0;
  static DisplayMode lastMode = MODE_MAIN;
  static bool lastWiFiStatus = false;
  static SolarIndices lastSolar = {0,0,0,0};
  static WeatherInfo lastWeather = {0.0f, 0.0f, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f, 0, "", "", "", "", "", 0, 0L, 0L, 0, false};
  
  // Screen saver state
  static bool screenSaverActive = false;
  static unsigned long lastActivity = 0;
  static unsigned long lastPixelUpdate = 0;
  static bool activityInitialized = false;
  
  // Auto page change state
  static unsigned long lastAutoPageSwitch = 0;
  
  // Loop performance monitoring
  static unsigned long lastLoopStart = 0;
  static unsigned long slowestLoop = 0;
  
  unsigned long now = millis();
  
  // Track slow loop iterations (>20ms is unusual)
  if (lastLoopStart > 0) {
    unsigned long loopTime = now - lastLoopStart;
    if (loopTime > 20) {
      Serial.printf("⚠ Main loop took %lu ms\n", loopTime);
    }
    if (loopTime > slowestLoop) {
      slowestLoop = loopTime;
    }
  }
  lastLoopStart = now;
  bool needsRedraw = false;
  
  // One-time init of activity timer
  if (!activityInitialized) {
    lastActivity = now;
    lastAutoPageSwitch = now;
    activityInitialized = true;
  }
  
  // Queue component updates to background worker every 250ms.
  // The worker does the actual (potentially blocking) HTTP fetch — main loop never blocks.
  static unsigned long lastComponentUpdate = 0;
  static int nextComponent = 0;
  if (now - lastComponentUpdate >= 250) {
    lastComponentUpdate = now;
    if (WiFi.status() == WL_CONNECTED && pendingComponentUpdate < 0) {
      // Worker is idle — hand it the next component
      pendingComponentUpdate = nextComponent;
      nextComponent = (nextComponent + 1) % 4;
    }
  }

  // Check WiFi status + data changes every 1 second
  if (now - lastUpdate >= 1000) {
    lastUpdate = now;
    
    bool currentWiFi = (WiFi.status() == WL_CONNECTED);
    
    // Check if WiFi status changed
    if (currentWiFi != lastWiFiStatus) {
      lastWiFiStatus = currentWiFi;
      needsRedraw = true;
    }
    
    // Check if data changed
    if (solarData) {
      SolarIndices currentSolar = solarData->getIndices();
      if (currentSolar.sfi != lastSolar.sfi || currentSolar.ssn != lastSolar.ssn) {
        lastSolar = currentSolar;
        needsRedraw = true;
      }
    }
    
    if (weatherData) {
      WeatherInfo currentWeather = weatherData->getWeather();
      if (currentWeather.tempF != lastWeather.tempF) {
        lastWeather = currentWeather;
        needsRedraw = true;
      }
    }
  }
  
  // Handle web requests every 200ms
  if (now - lastWebCheck >= 200) {
    lastWebCheck = now;
    server.handleClient();
  }
  
  // ============================================================
  // SCREEN SAVER MODE
  // ============================================================
  if (screenSaverActive) {
    // Animate random pixels every second
    if (now - lastPixelUpdate >= 1000) {
      lastPixelUpdate = now;
      tft.fillScreen(TFT_BLACK);
      for (int i = 0; i < 200; i++) {
        tft.drawPixel(random(320), random(240),
                      tft.color565(random(256), random(256), random(256)));
      }
    }
    // Exit screensaver on any touch
    if (touch && touch->checkTouch()) {
      Serial.println("🖐 Touch detected — exiting screensaver");
      screenSaverActive = false;
      lastActivity = now;
      touch->setMode((DisplayMode)lastMode);  // Reset mode — don't let wake-up touch also cycle pages
      tft.fillScreen(TFT_BLACK);
      needsRedraw = true;
      if (display) {
        display->setPage((DisplayPage)lastMode);
        display->begin();  // Reset page state so it redraws fully
      }
    }
    return;  // Skip everything else while in screensaver
  }
  
  // Handle touch input frequently (every 50ms) for responsiveness
  if (touch && (now - lastTouchCheck >= 50)) {
    lastTouchCheck = now;
    if (touch->checkTouch()) {
      lastActivity = now;  // Any touch resets the inactivity timer
    }
    DisplayMode currentMode = touch->getCurrentMode();
    
    // Check if page changed
    if (currentMode != lastMode) {
      Serial.printf("Page changed to: %d (from: %d)\n", currentMode, lastMode);
      
      // If returning to MODE_MAIN (0) from any other page, force full redraw
      if (currentMode == MODE_MAIN && lastMode != MODE_MAIN) {
        Serial.println("→ Returning to MAIN - forcing full screen refresh");
        if (display) {
          display->begin();  // Reset display state
          display->forceMapRedraw();  // Force map redraw
        }
      }
      
      lastMode = currentMode;
      needsRedraw = true;
      
      // Force immediate display update on page change
      if (display) {
        display->setPage((DisplayPage)lastMode);
        display->drawCurrentPage(
          lastWiFiStatus,
          true,
          solarData,
          weatherData,
          pskReporter,
          issTracker ? issTracker->getLatitude() : 0.0,
          issTracker ? issTracker->getLongitude() : 0.0,
		      issTracker,
		      true  // ADD THIS
        );
    
        lastDisplayUpdate = now;
      }
      return; // Skip normal redraw logic this cycle
    }
  }
  
  // ============================================================
  // INACTIVITY → SCREENSAVER
  // ============================================================
  if (config.screenSaverTimeout > 0 && (now - lastActivity > (unsigned long)config.screenSaverTimeout)) {
    Serial.println("⏳ Inactivity detected — entering screensaver");
    screenSaverActive = true;
    tft.fillScreen(TFT_BLACK);
    lastPixelUpdate = 0;  // Force immediate first pixel frame
    return;
  }
  
  // ============================================================
  // AUTO PAGE CHANGE (cycles MAIN ↔ PROPAGATION every 15s)
  // ============================================================
  if (config.autoPageChange) {
    if (now - lastAutoPageSwitch >= 15000) {
      lastAutoPageSwitch = now;
      if (lastMode == MODE_MAIN) {
        lastMode = MODE_PROPAGATION;
      } else {
        lastMode = MODE_MAIN;
      }
      Serial.printf("🔀 Auto page change → mode %d\n", lastMode);
      needsRedraw = true;
      if (display) {
        display->setPage((DisplayPage)lastMode);
        if (lastMode == MODE_MAIN) {
          display->begin();           // Reset display state
          display->forceMapRedraw();  // Force world map + greyline redraw
        }
      }
      if (touch) touch->setMode(lastMode);  // Keep touch handler in sync or it reverts us next loop
    }
  }
  
  // 🖼 Splash screen — painted here (not in setup()) so that WiFi init,
  // QR codes, and other setup drawing don't overwrite it before we can hold it.
  if (splashActive) {
    if (splashDisplayedAt == 0) {
      // First time through — paint it now and start the timer
      Serial.println("🖼  Painting splash screen");
      tft.fillScreen(TFT_BLACK);
      displayPNGfromLittleFS(config.startupLogo, 0);
      splashDisplayedAt = millis();
    }
    if (now - splashDisplayedAt < (unsigned long)config.splashDuration) {
      return;  // Hold — keep splash visible, skip all drawing
    }
    // Minimum time met — clear splash and draw first page clean
    splashActive = false;
    tft.fillScreen(TFT_BLACK);
    needsRedraw = true;
    Serial.println("🖼  Splash duration elapsed — drawing first page");
  }
  
  // Redraw display if something changed OR every 10ms for smooth banner/clock updates
  if (needsRedraw || (now - lastDisplayUpdate >= 10)) {
    lastDisplayUpdate = now;
    
    if (display) {
      display->setPage((DisplayPage)lastMode);
      display->drawCurrentPage(
        lastWiFiStatus,
        true,
          solarData,
          weatherData,
        pskReporter,
        issTracker ? issTracker->getLatitude() : 0.0,
        issTracker ? issTracker->getLongitude() : 0.0,
		    issTracker,
		    true  // ADD THIS
      );
    }
  }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
bool connectToWiFi() {
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(&DINPro_Regular6pt8b);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(40, 60);
  tft.print("Connecting to");
  tft.setCursor(80, 90);
  tft.print(wifiConfig.ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiConfig.ssid, wifiConfig.password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n✅ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(80, 80);
    tft.print("Connected!");
    tft.setCursor(20, 120);
    tft.print("IP: " + WiFi.localIP().toString());
    
    
    // Start mDNS responder
    if (MDNS.begin("hamclock")) {
      Serial.println("✅ mDNS responder started");
      Serial.println("   Access via: http://hamclock.local");
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("⚠  Error starting mDNS responder");
    }
    delay(2000);
    return true;
  } else {
    Serial.println("\n❌ Failed to connect");
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(40, 80);
    tft.print("Connect Failed");
    
    delay(3000);
    return false;
  }
}

void showError(const char* message) {
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(&DINPro_Regular6pt8b);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setCursor(40, 120);
  tft.print("ERROR:");
  tft.setCursor(40, 140);
  tft.print(message);
}

void listLittleFSFiles() {
  Serial.println("📂 LittleFS Files:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
    file = root.openNextFile();
  }
}
