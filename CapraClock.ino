/*************************************************************
 * CapraClock - WiFi Clock with ESP32 and LED matrix
 * Hardware: ESP32 + WS2812B 8x32 LED matrix
 * Required Libraries:
 * - Adafruit GFX, Adafruit NeoMatrix, Adafruit NeoPixel
 * - WiFi (included with ESP32)
 * Required files:
 * - CapraFont (custom font)
 * - settings.h (custom settings)
 *************************************************************/

/*************************************************************
 * | GPIO | USAGE        |
 * |------|--------------|
 * |  32  | LED MATRIX   |
 * |  22  | SCL (Temp)   |
 * |  21  | SDA (Temp)   |
 * |  04  | BRIGHTNESS   |
 *************************************************************/

// ==== LIBRARIES ==== 
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <CapraFont.h>
#include <WiFi.h>
#include <time.h>
#include <WebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_HTU21DF.h>

// ==== SETTINGS ==== 
#include "settings.h"


// ==== DISPLAY ==== 
const unsigned int MATRIX_PIN = 32;

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(
  32, 8, MATRIX_PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + 
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800
);

// Clock position on the matrix
int positionClock = 2;

// Screen display duration
unsigned long screenDuration[] = {600, 10, 10};  // [seconds]
const int numScreens = sizeof(screenDuration) / sizeof(screenDuration[0]);
unsigned long lastSwitch = 0;
int currentScreen = 0;

// Brightness levels
int brightnessDayHigh   = 30;   // High brightness during the day
int brightnessDayLow    = 20;   // Low brightness during the day
int brightnessNightHigh = 20;   // High brightness during the night
int brightnessNightLow  = 3;    // Low brightness during the night

int currentBrightness = brightnessDayLow;

// Colors (r,g,b)
uint32_t backgroundTimeDay     = matrix.Color( 14, 152, 227);  // Time background color (Day)
uint32_t backgroundTimeNight   = matrix.Color(0,     0,   0);  // Time background color (Night)
uint32_t timeColorDay          = matrix.Color(255, 255, 255);  // Time text color (Day)
uint32_t timeColorNight        = matrix.Color(255,   0,   0);  // Time text color (Night)

uint32_t backgroundDateDay     = matrix.Color( 14, 152, 227);  // Date background color (Day)
uint32_t backgroundDateNight   = matrix.Color(0,     0,   0);  // Date background color (Night)
uint32_t dateColorDay          = matrix.Color(255, 255, 255);  // Date text color (Day)
uint32_t dateColorNight        = matrix.Color(255,   0,   0);  // Date text color (Night)

uint32_t backgroundTempDay     = matrix.Color( 14, 152, 227);  // Temperature/Humidity background color (Day)
uint32_t backgroundTempNight   = matrix.Color(0,     0,   0);  // Temperature/Humidity background color (Night)
uint32_t tempColorDay          = matrix.Color(255, 255, 255);  // Temperature/Humidity text color (Day)
uint32_t tempColorNight        = matrix.Color(255,   0,   0);  // Temperature/Humidity text color (Night)

uint32_t dayColorDay           = matrix.Color(255, 255, 255);  // Current weekday indicator color (Day)
uint32_t dayColorNight         = matrix.Color(255,   0,   0);  // Current weekday indicator color (Night)
uint32_t notDayColorDay        = matrix.Color( 80,  80,  80);  // Other weekdays indicator color (Day)
uint32_t notDayColorNight      = matrix.Color(172,   0,   0);  // Other weekdays indicator color (Night)

bool dayColors = true; // true = use day colors, false = use night colors (Note: Original comment was confusing)


// ==== LUMINOSITY SENSOR ===
const unsigned int LUX_PIN = 34;
int luminosita = 0;

unsigned long tempLuxInterval = 1; // Read interval (seconds)
unsigned long lastLuxRead = 0;

unsigned int thresholdLuxDay = 1500;
unsigned int thresholdLuxNight = 1500;


// ==== TEMPERATURE AND HUMIDITY SENSOR ====
Adafruit_HTU21DF htu = Adafruit_HTU21DF();
float temp = 0;
float hum = 0;

float tempOffset = 0;    // Offset for temperature sensor calibration
float humOffset = 0;     // Offset for humidity sensor calibration

unsigned long tempReadInterval = 60; // Read interval (seconds)
unsigned long lastTempRead = 0;

// Buffer for formatting temperature and humidity string
String tempStr;


// ==== WEBSERVER AND SETTINGS ==== 
WebServer server(80);
Preferences preferences;


// ==== WIFI ====
// Maximum timeout to attempt WiFi connection (ms)
const unsigned long timeoutConnection = 10000;
const unsigned int MAXSSIDFOUND = 10;

const int sizeHomeSSID = sizeof(HomeSSID) / sizeof(HomeSSID[0]);

// Buffers for found networks
String foundSSID[MAXSSIDFOUND];
String foundPASS[MAXSSIDFOUND];
int foundRSSI[MAXSSIDFOUND];
int foundCount = 0;
String connectedSSID;
bool connected = false;


// ==== DATE AND TIME ==== 
// NTP Servers
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* ntpServer3 = "time.google.com";

// Italian time zone (including DST)
const char* timeZoneItaly = "CET-1CEST,M3.5.0,M10.5.0/3";

// Synchronization every 10 minutes (600 sec)
unsigned long syncInterval = 600;
unsigned long lastSync = 0;

// Offset for second correction
int timeOffset = -2;

// Day/Night start times (HH:MM)
int dayStartHour     = 7;
int dayStartMinute   = 0;
int nightStartHour   = 22;
int nightStartMinute = 0;

struct tm timeinfo;
time_t now;

// Buffer for formatting time and date strings
char timeStr[9];


// ==== SETUP ==== 
void setup() {
  delay(2000);

  // Serial debug initialization
  Serial.begin(115200);
  while (!Serial) delay(1000);

  Serial.println("\n\n");

  // Initialize the LED matrix
  Serial.println("Initializing display...");
  matrix.begin();
  matrix.clear();
  matrix.setFont(&CapraFont);
  matrix.setTextWrap(false);
  matrix.setBrightness(brightnessDayHigh);
  matrix.setTextColor(timeColorDay);
  matrix.show();

  // Initialize LittleFS
  mountLittleFS();
  
  // Load saved settings
  loadSettings();

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);

  // Scan networks and attempt connection
  scanWifi();
  sortByRSSI();
  tryConnection();

  // Synchronize time
  syncTime();

  // Initialize temperature and humidity sensor
  if (ENABLE_SENSORS == true){
    initTempHum();
    readTempHum();
  }

  // Luminosity sensor setup
  analogReadResolution(12);
  updateBrightnessAndColors();

  // Start web server
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("Webserver started.");

  // Display logo
  if (SHOW_LOGO == true)
    scrollText("CapraClock");
}


// ==== LOOP ==== 
void loop() {
    // Synchronize time periodically
  if (millis() - lastSync > syncInterval * 1000) 
    syncTime();

  // Read temperature and humidity periodically
  if ((ENABLE_SENSORS == true) && (millis() - lastTempRead >= tempReadInterval * 1000)){
    readTempHum();
    lastTempRead = millis();
  }

  // Update brightness and colors based on light level
  if (millis() - lastLuxRead >= tempLuxInterval * 1000){
    updateBrightnessAndColors();
    lastLuxRead = millis();
  }

  // Switch screen display
  if (millis() - lastSwitch >= screenDuration[currentScreen] * 1000) {
    switch (currentScreen){
      case 0:
        if (ENABLE_DATE == true) {
          currentScreen = 1;
        } else if (ENABLE_SENSORS == true) { // Usa "else if"
          currentScreen = 2;
        } else {
          currentScreen = 0; // Rimani esplicitamente su 0
        }
        break;

      case 1:
        if (ENABLE_SENSORS == true){
          currentScreen = 2;
        }  
        else{
          currentScreen = 0;          
        }
        break;

      case 2:
        currentScreen = 0;
        break;

      default: 
        currentScreen = 0;
        break;
    }
    lastSwitch = millis();
  }

  // Display current screen
  switch (currentScreen) {
    case 0: 
      showTime(); 
      showDay(); 
      break;
    case 1:
      showDate();
      showDay(); 
      break;
    case 2:
      showTempHum();
      showDay();
      break;
  }
  matrix.show();

  // Handle web server requests
  server.handleClient();

  // delay(1); // Small delay for loop stability (often not needed with ESP32)
}


// ==== BRIGHTNESS FUNCTIONS ==== 
void readLux(){
  luminosita = analogRead(LUX_PIN);
}   

void updateBrightnessAndColors() {
  readLux();

  bool day = isDaytime(&timeinfo);

  if (day) {
    if (luminosita < thresholdLuxDay) {
      currentBrightness = brightnessDayLow;
    } else {
      currentBrightness = brightnessDayHigh;
    }
    dayColors = true;   // Use day colors during the day
  } else {
    if (luminosita < thresholdLuxNight) {
      currentBrightness = brightnessNightLow;
      dayColors = false;   // Low light at night = use night colors
    } else {
      currentBrightness = brightnessNightHigh;
      dayColors = true;   // High light at night = use day colors (e.g., if a light is turned on)
    }
  }
}


// ==== TEMPERATURE AND HUMIDITY FUNCTIONS ==== 
void initTempHum(){
  if (!htu.begin()) {
    Serial.println("HTU21D not found");
    while (1) scrollText("No sensor found"); // Loop indefinitely showing error
  }
}

void readTempHum(){
  temp = htu.readTemperature();
  hum = htu.readHumidity();

  temp += tempOffset; // Apply calibration offset
  hum += humOffset;   // Apply calibration offset

  Serial.print("Temperature = ");
  Serial.print(temp);
  Serial.print("\t Humidity = ");
  Serial.println(hum);
}

void showTempHum() {
  // Format string: Temp° Hum%
  tempStr = String(temp,1) + String("\xB0 ") + String(hum,0) + String("%");

  matrix.setBrightness(currentBrightness);
  background(dayColors ? backgroundTempDay : backgroundTempNight);
  matrix.setCursor(1, 6);
  matrix.setTextColor(dayColors ? tempColorDay : tempColorNight);
  matrix.print(tempStr);
}


// ==== TIME/DATE FUNCTIONS ==== 
void syncTime() {
  Serial.print("Synchronizing time... ");
  // Configure time zone and start NTP client
  configTzTime(timeZoneItaly, ntpServer1, ntpServer2, ntpServer3);

  // Get current time structure
  getLocalTime(&timeinfo); 
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  Serial.println(timeStr);

  lastSync = millis();
}

// Check if current time is within the defined "daytime" period
bool isDaytime(struct tm *t) {
  int currentMinutes = t->tm_hour * 60 + t->tm_min;
  int dayStart   = dayStartHour * 60 + dayStartMinute;
  int nightStart = nightStartHour * 60 + nightStartMinute;

  if (dayStart < nightStart) {
    // Normal case: daytime within the same day (e.g., 07:00–22:00)
    return (currentMinutes >= dayStart && currentMinutes < nightStart);
  } else {
    // Inverted case: daytime crosses midnight (e.g., 22:00–07:00)
    return (currentMinutes >= dayStart || currentMinutes < nightStart);
  }
}


// ==== SCREEN DISPLAY FUNCTIONS ====
void showTime() {
  if (getLocalTime(&timeinfo)) {
    // Apply time offset for minor adjustments
    time(&now);
    now += timeOffset;
    localtime_r(&now, &timeinfo);

    // Format time string (HH:MM:SS)
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

    matrix.setBrightness(currentBrightness);
    background(dayColors ? backgroundTimeDay : backgroundTimeNight);
    matrix.setCursor(positionClock, 6);
    matrix.setTextColor(dayColors ? timeColorDay : timeColorNight);
    matrix.print(timeStr);
  } else {
    Serial.println("Error: Unable to get time.");
    matrix.clear();
    matrix.show();
  }
}

void showDate() {
  if (getLocalTime(&timeinfo)) {
    char dateStr[9];  // "dd.mm.yy"
    strftime(dateStr, sizeof(dateStr), "%d.%m.%y", &timeinfo);

    matrix.setBrightness(currentBrightness);
    background(dayColors ? backgroundDateDay : backgroundDateNight);
    matrix.setCursor(positionClock, 6);
    matrix.setTextColor(dayColors ? dateColorDay : dateColorNight);
    matrix.print(dateStr);
  } else {
    Serial.println("Error: Unable to get date.");
    matrix.clear();
    matrix.show();
  }
}

// Displays a bar indicating the day of the week
void showDay() {
  if (getLocalTime(&timeinfo)) {
    int rectWidth = 3;
    int rectHeight = 1;
    int spacing = 1;
    int startX = positionClock;
    int y = 7; // last row

    matrix.setBrightness(currentBrightness);

    // tm_wday is 0 (Sunday) to 6 (Saturday). We map 0..6 to 7 blocks.
    // (timeinfo.tm_wday + 6) % 7 maps Monday (1) to block 0, Tuesday (2) to block 1, ..., Sunday (0) to block 6.
    for (int d = 0; d <= 6; d++) {
      int x = startX + d * (rectWidth + spacing);
      uint32_t color = (d == (timeinfo.tm_wday + 6) % 7) ? (dayColors ? dayColorDay : dayColorNight) : (dayColors ? notDayColorDay : notDayColorNight);
      
      matrix.fillRect(x, y, rectWidth, rectHeight, color);
    }
  } else {
    Serial.println("Error: Unable to get weekday");
    matrix.clear();
    matrix.show();
  }
}


// Fills the screen background, leaving the bottom row clear for the day indicator.
void background(uint32_t color){
  int rectWidth = 3;
  int rectHeight = 1;
  int spacing = 1;
  int startX = positionClock;
  int y = 7; // last row

  matrix.setBrightness(currentBrightness);
  // Fill the main display area (rows 0-6)
  matrix.fillRect(0,0,matrix.width(),matrix.height()-1, color);

  // Fill the empty spaces on the bottom row (row 7) for a uniform look
  // These fill regions are complex due to the showDay implementation
  matrix.fillRect(0, y, startX, rectHeight, color);
  matrix.fillRect(startX+rectWidth*1+spacing*0, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*2+spacing*1, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*3+spacing*2, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*4+spacing*3, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*5+spacing*4, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*6+spacing*5, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*7+spacing*6, y, matrix.width()-startX+rectWidth*7+spacing*6, rectHeight, color);
}


// ==== SETTINGS FUNCTIONS ==== 
void loadSettings() {   // Load settings from Preferences (with defaults)
  preferences.begin("settings", true);

  // Clock position
  positionClock       = preferences.getUInt("positionClock", positionClock);

  // Screen display duration
  screenDuration[0]   = preferences.getUInt("screenClock", screenDuration[0]);
  screenDuration[1]   = preferences.getUInt("screenDate", screenDuration[1]);
  screenDuration[2]   = preferences.getUInt("screenTemp", screenDuration[2]);

  // Brightness levels
  brightnessDayHigh   = preferences.getUInt("brightnessDayHigh", brightnessDayHigh);
  brightnessDayLow    = preferences.getUInt("brightnessDayLow", brightnessDayLow);
  brightnessNightHigh = preferences.getUInt("brightnessNightHigh", brightnessNightHigh);
  brightnessNightLow  = preferences.getUInt("brightnessNightLow", brightnessNightLow);

  // Colors (r,g,b)
  backgroundTimeDay   = preferences.getUInt("backgroundTimeDay", backgroundTimeDay);
  backgroundTimeNight = preferences.getUInt("backgroundTimeNight", backgroundTimeNight);
  timeColorDay        = preferences.getUInt("timeColorDay", timeColorDay);
  timeColorNight      = preferences.getUInt("timeColorNight", timeColorNight);

  backgroundDateDay   = preferences.getUInt("backgroundDateDay", backgroundDateDay);
  backgroundDateNight = preferences.getUInt("backgroundDateNight", backgroundDateNight);
  dateColorDay        = preferences.getUInt("dateColorDay", dateColorDay);
  dateColorNight      = preferences.getUInt("dateColorNight", dateColorNight);

  backgroundTempDay   = preferences.getUInt("backgroundTempDay", backgroundTempDay);
  backgroundTempNight = preferences.getUInt("backgroundTempNight", backgroundTempNight);
  tempColorDay        = preferences.getUInt("tempColorDay", tempColorDay);
  tempColorNight      = preferences.getUInt("tempColorNight", tempColorNight);

  dayColorDay         = preferences.getUInt("dayColorDay", dayColorDay);
  dayColorNight       = preferences.getUInt("dayColorNight", dayColorNight);
  notDayColorDay      = preferences.getUInt("notDayColorDay", notDayColorDay);
  notDayColorNight    = preferences.getUInt("dayColorNight", notDayColorNight);

  // Luminosity settings
  thresholdLuxDay     = preferences.getUInt("thresholdLuxDay", thresholdLuxDay);
  thresholdLuxNight   = preferences.getUInt("thresholdLuxNight", thresholdLuxNight);

  // Temperature and humidity offsets
  tempOffset          = preferences.getFloat("tempOffset", tempOffset);
  humOffset           = preferences.getFloat("humOffset", humOffset);

  tempReadInterval    = preferences.getInt("tempReadInterval", tempReadInterval);

  // Time and Date settings
  syncInterval        = preferences.getUInt("syncInterval", syncInterval);
  timeOffset          = preferences.getInt("timeOffset", timeOffset);

  // Day/Night start times (HH:MM format)
  dayStartHour        = preferences.getUInt("dayStartHour", dayStartHour);
  dayStartMinute      = preferences.getUInt("dayStartMinute", dayStartMinute);
  nightStartHour      = preferences.getUInt("nightStartHour", nightStartHour);
  nightStartMinute    = preferences.getUInt("nightStartMinute", nightStartMinute);

  // Enable or disable screen
  ENABLE_DATE = preferences.getBool("ENABLE_DATE", ENABLE_DATE);
  ENABLE_SENSORS = preferences.getBool("ENABLE_SENSORS", ENABLE_SENSORS);

  preferences.end();
}

void saveSettings() { // Save settings to Preferences
  preferences.begin("settings", false);

  // Clock position
  preferences.putUInt("positionClock", positionClock);
  
  // Screen display duration
  preferences.putUInt("screenClock", screenDuration[0]);
  preferences.putUInt("screenDate", screenDuration[1]);
  preferences.putUInt("screenTemp", screenDuration[2]);

  // Brightness levels
  preferences.putUInt("brightnessDayHigh", brightnessDayHigh);
  preferences.putUInt("brightnessDayLow", brightnessDayLow);
  preferences.putUInt("brightnessNightHigh", brightnessNightHigh);
  preferences.putUInt("brightnessNightLow", brightnessNightLow);

  // Colors (r,g,b)
  preferences.putUInt("backgroundTimeDay", backgroundTimeDay);
  preferences.putUInt("backgroundTimeNight", backgroundTimeNight);
  preferences.putUInt("timeColorDay", timeColorDay);
  preferences.putUInt("timeColorNight", timeColorNight);

  preferences.putUInt("backgroundDateDay", backgroundDateDay);
  preferences.putUInt("backgroundDateNight", backgroundDateNight);
  preferences.putUInt("dateColorDay", dateColorDay);
  preferences.putUInt("dateColorNight", dateColorNight);

  preferences.putUInt("backgroundTempDay", backgroundTempDay);
  preferences.putUInt("backgroundTempNight", backgroundTempNight);
  preferences.putUInt("tempColorDay", tempColorDay);
  preferences.putUInt("tempColorNight", tempColorNight);

  preferences.putUInt("dayColorDay", dayColorDay);
  preferences.putUInt("dayColorNight", dayColorNight);
  preferences.putUInt("notDayColorDay", notDayColorDay);
  preferences.putUInt("notDayColorNight", notDayColorNight);

  // Luminosity settings
  preferences.putUInt("thresholdLuxDay", thresholdLuxDay);
  preferences.putUInt("thresholdLuxNight", thresholdLuxNight);

  // Temperature and humidity offsets
  preferences.putFloat("tempOffset", tempOffset);
  preferences.putFloat("humOffset", humOffset);

  preferences.putUInt("tempReadInterval", tempReadInterval);

  // Time and Date settings
  preferences.putUInt("syncInterval", syncInterval);
  preferences.putInt("timeOffset", timeOffset);

  // Day/Night start times (HH:MM format)
  preferences.putUInt("dayStartHour", dayStartHour);
  preferences.putUInt("dayStartMinute", dayStartMinute);
  preferences.putUInt("nightStartHour", nightStartHour);
  preferences.putUInt("nightStartMinute", nightStartMinute);

  // Enable or disable screen
  preferences.putBool("ENABLE_DATE", ENABLE_DATE);
  preferences.putBool("ENABLE_SENSORS", ENABLE_SENSORS);

  preferences.end();
}


// ==== LITTLE FS ==== 
void mountLittleFS(){
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Error");
    return;
  }

  // List files present in LittleFS
  Serial.println("Files present in LittleFS:");
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("Error: Invalid root directory");
  } else {
    File file = root.openNextFile();
    while (file) {
      Serial.printf(" - %s (%d bytes)\n", file.name(), file.size());
      file = root.openNextFile();
    }
  }
}


// ==== WEB SERVER FUNCTIONS ==== 
void handleRoot() {
  String html;
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Error: HTML file not found");
    return;
  }

  html = file.readString();
  file.close();
  // Replace placeholders with current values

  // Clock position
  html.replace("%POSITION_CLOCK%", String(positionClock));

  // Screen display duration
  html.replace("%SCREEN_CLOCK%", String(screenDuration[0]));
  html.replace("%SCREEN_DATE%", String(screenDuration[1]));
  html.replace("%SCREEN_TEMP%", String(screenDuration[2]));
  
  // Brightness levels
  html.replace("%BRIGHTNESS_DAY_HIGH%", String(brightnessDayHigh));
  html.replace("%BRIGHTNESS_DAY_LOW%", String(brightnessDayLow));
  html.replace("%BRIGHTNESS_NIGHT_HIGH%", String(brightnessNightHigh));
  html.replace("%BRIGHTNESS_NIGHT_LOW%", String(brightnessNightLow));

  // Colors (r,g,b) in hex format
  html.replace("%TIME_BACKGROUND_COLOR_DAY%", toHexColor(backgroundTimeDay));
  html.replace("%TIME_BACKGROUND_COLOR_NIGHT%", toHexColor(backgroundTimeNight));
  html.replace("%TIME_COLOR_DAY%", toHexColor(timeColorDay));
  html.replace("%TIME_COLOR_NIGHT%", toHexColor(timeColorNight));

  html.replace("%DATE_BACKGROUND_COLOR_DAY%", toHexColor(backgroundDateDay));
  html.replace("%DATE_BACKGROUND_COLOR_NIGHT%", toHexColor(backgroundDateNight));
  html.replace("%DATE_COLOR_DAY%", toHexColor(dateColorDay));
  html.replace("%DATE_COLOR_NIGHT%", toHexColor(dateColorNight));

  html.replace("%TEMP_BACKGROUND_COLOR_DAY%", toHexColor(backgroundTempDay));
  html.replace("%TEMP_BACKGROUND_COLOR_NIGHT%", toHexColor(backgroundTempNight));
  html.replace("%TEMP_COLOR_DAY%", toHexColor(tempColorDay));
  html.replace("%TEMP_COLOR_NIGHT%", toHexColor(tempColorNight));

  html.replace("%DAY_COLOR_DAY%", toHexColor(dayColorDay));
  html.replace("%DAY_COLOR_NIGHT%", toHexColor(dayColorNight));
  html.replace("%NOT_DAY_COLOR_DAY%", toHexColor(notDayColorDay));
  html.replace("%NOT_DAY_COLOR_NIGHT%", toHexColor(notDayColorNight));

  // Luminosity thresholds
  html.replace("%THRESHOLD_LUX_DAY%", String(thresholdLuxDay));
  html.replace("%THRESHOLD_LUX_NIGHT%", String(thresholdLuxNight));

  // Temperature and humidity offsets
  html.replace("%TEMP_OFFSET%", String(tempOffset));
  html.replace("%HUM_OFFSET%", String(humOffset));
  
  html.replace("%TEMP_INTERVAL%", String(tempReadInterval));

  // Time and Date settings
  html.replace("%SYNC_INTERVAL%", String(syncInterval));
  html.replace("%TIME_OFFSET%", String(timeOffset));

  // Day/Night start times (HH:MM format)
  char buf[6];
  sprintf(buf, "%02d:%02d", dayStartHour, dayStartMinute);
  html.replace("%DAY_START%", String(buf));
  sprintf(buf, "%02d:%02d", nightStartHour, nightStartMinute);
  html.replace("%NIGHT_START%", String(buf));

  // Enable or disable screen
  html.replace("%ENABLE_DATE_CHECKED%", ENABLE_DATE ? "checked" : "");
  html.replace("%ENABLE_SENSORS_CHECKED%", ENABLE_SENSORS ? "checked" : "");

  server.send(200, "text/html", html);
}

void handleSave() {
  // Read and save numerical values

  // Clock position
  positionClock       = server.arg("positionClock").toInt();

  // Screen display duration
  screenDuration[0]   = server.arg("screenClock").toInt();
  screenDuration[1]   = server.arg("screenDate").toInt();
  screenDuration[2]   = server.arg("screenTemp").toInt();

  // Brightness levels
  brightnessDayHigh   = server.arg("brightnessDayHigh").toInt();
  brightnessDayLow    = server.arg("brightnessDayLow").toInt();
  brightnessNightHigh = server.arg("brightnessNightHigh").toInt();
  brightnessNightLow  = server.arg("brightnessNightLow").toInt();

  // Colors (r,g,b) - convert HTML hex to matrix format
  backgroundTimeDay   = htmlColorToMatrix(server.arg("backgroundTimeDay"));
  backgroundTimeNight = htmlColorToMatrix(server.arg("backgroundTimeNight"));
  timeColorDay        = htmlColorToMatrix(server.arg("timeColorDay"));
  timeColorNight      = htmlColorToMatrix(server.arg("timeColorNight"));

  backgroundDateDay   = htmlColorToMatrix(server.arg("backgroundDateDay"));
  backgroundDateNight = htmlColorToMatrix(server.arg("backgroundDateNight"));
  dateColorDay        = htmlColorToMatrix(server.arg("dateColorDay"));
  dateColorNight      = htmlColorToMatrix(server.arg("dateColorNight"));

  backgroundTempDay   = htmlColorToMatrix(server.arg("backgroundTempDay"));
  backgroundTempNight = htmlColorToMatrix(server.arg("backgroundTempNight"));
  tempColorDay        = htmlColorToMatrix(server.arg("tempColorDay"));
  tempColorNight      = htmlColorToMatrix(server.arg("tempColorNight"));

  dayColorDay         = htmlColorToMatrix(server.arg("dayColorDay"));
  dayColorNight       = htmlColorToMatrix(server.arg("dayColorNight"));
  notDayColorDay      = htmlColorToMatrix(server.arg("notDayColorDay"));
  notDayColorNight    = htmlColorToMatrix(server.arg("notDayColorNight"));

  // Luminosity thresholds
  thresholdLuxDay     = server.arg("thresholdLuxDay").toInt();
  thresholdLuxNight   = server.arg("thresholdLuxNight").toInt();

  // Temperature and humidity offsets
  tempOffset          = server.arg("tempOffset").toInt();
  humOffset           = server.arg("humOffset").toInt();

  tempReadInterval    = server.arg("tempReadInterval").toInt();

  // Time and Date settings
  syncInterval        = server.arg("syncInterval").toInt();
  timeOffset          = server.arg("timeOffset").toInt();

  // Day/Night start times (HH:MM format)
  if (server.hasArg("dayStart")) {
    sscanf(server.arg("dayStart").c_str(), "%d:%d", &dayStartHour, &dayStartMinute);
  }
  if (server.hasArg("nightStart")) {
    sscanf(server.arg("nightStart").c_str(), "%d:%d", &nightStartHour, &nightStartMinute);
  }

  // Enable or disable screen
  ENABLE_DATE = server.hasArg("enableDate");
  ENABLE_SENSORS = server.hasArg("enableSensors");

  saveSettings();

  // Redirect to root after saving
  server.sendHeader("Location", "/");
  server.send(303);
}

// Converts HTML color string (e.g., "#RRGGBB") to NeoMatrix color format
uint32_t htmlColorToMatrix(String hexColor) {
  if (hexColor[0] == '#') hexColor = hexColor.substring(1);
  long value = strtol(hexColor.c_str(), NULL, 16);
  uint8_t r = (value >> 16) & 0xFF;
  uint8_t g = (value >> 8) & 0xFF;
  uint8_t b = value & 0xFF;
  return matrix.Color(r, g, b);
}

// Converts a matrix color (uint16_t in 565 format) to HTML hex color string (#RRGGBB)
String toHexColor(uint16_t color565) {
  // Extract RGB components from 565 format
  uint8_t r5 = (color565 >> 11) & 0x1F;
  uint8_t g6 = (color565 >> 5)  & 0x3F;
  uint8_t b5 = color565 & 0x1F;

  // Convert to 8-bit components (0-255)
  uint8_t r8 = (r5 * 255) / 31;
  uint8_t g8 = (g6 * 255) / 63;
  uint8_t b8 = (b5 * 255) / 31;

  char hexCol[8];
  sprintf(hexCol, "#%02X%02X%02X", r8, g8, b8);

  return String(hexCol);
}


// ==== WIFI FUNCTIONS ==== 
void scanWifi() {
  Serial.println("Scanning WiFi networks...");
  int n = WiFi.scanNetworks();
  Serial.printf("Found %d networks.\n", n);

  if (n == 0) {
    Serial.println("No WiFi networks found.");
    while (true) scrollText("No WiFi found"); // Fatal error loop
  }

  // Save only the configured "home" networks found
  for (int i = 0; i < n && foundCount < MAXSSIDFOUND; i++) {
    for (int k = 0; k < sizeHomeSSID; k++) {
      if (WiFi.SSID(i) == HomeSSID[k]) {
        Serial.printf("Found home network %s (RSSI=%ld)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        foundSSID[foundCount] = WiFi.SSID(i);
        foundRSSI[foundCount] = WiFi.RSSI(i);
        foundCount++;
        break;
      }
    }
  }

  if (foundCount == 0) {
    Serial.println("No configured home WiFi networks found.");
    while (true) scrollText("No Home WiFi found"); // Fatal error loop
  }

  WiFi.scanDelete();
}

// Sorts found networks by RSSI (signal strength) in descending order
void sortByRSSI() {
  Serial.println("Sorting networks by signal strength...");
  // Bubble sort descending
  for (int i = 0; i < foundCount - 1; i++) {
    for (int j = 0; j < foundCount - i - 1; j++) {
      if (foundRSSI[j] < foundRSSI[j+1]) {
        std::swap(foundRSSI[j], foundRSSI[j+1]);
        std::swap(foundSSID[j], foundSSID[j+1]);
      }
    }
  }
  for (int i = 0; i < foundCount; i++)
    Serial.printf("%d) %s (RSSI=%ld)\n", i+1, foundSSID[i].c_str(), (long)foundRSSI[i]);
}

// Attempts connection to networks in sorted order
void tryConnection() {
  for (int i = 0; i < foundCount; i++) {
    Serial.printf("Connecting to %s...\n", foundSSID[i].c_str());
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(foundSSID[i].c_str(), HomePASS);

    unsigned long start = millis();
    while (millis() - start < timeoutConnection) {
      if (WiFi.status() == WL_CONNECTED) {
        connectedSSID = foundSSID[i];
        Serial.printf("Connected to %s - IP: %s\n", connectedSSID.c_str(), WiFi.localIP().toString().c_str());
        scrollText(connectedSSID + String(" - ") + WiFi.localIP().toString());
        return; // Success
      }
    }
    Serial.printf("Connection to %s failed.\n", foundSSID[i].c_str());
  }

  if (!connected) {
    Serial.println("Connection with all networks failed.");
    while (true) scrollText("Not connected"); // Fatal error loop
  }
}


// ==== SCROLLING TEXT FUNCTION ==== 
void scrollText(String text) {
  int textSize = text.length();
  int x = matrix.width();

  Serial.printf("Displaying scrolling text: %s\n", text.c_str());

  // Scroll until text is fully off screen (4 pixels per character)
  while (x > -(textSize * 4)) {
    background(0);
    matrix.setBrightness(brightnessDayHigh);
    matrix.setCursor(x, 6);
    matrix.print(text);
    matrix.show();
    x--;
    delay(40);
  }
}
