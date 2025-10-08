/*************************************************************
 *   CapraClock - Orologio WiFi con ESP32 e matrice LED
 *   Hardware: ESP32 + matrice LED WS2812B 8x32
 *   Librerie necessarie:
 *     - Adafruit GFX, Adafruit NeoMatrix, Adafruit NeoPixel
 *     - CapraFont (font personalizzato)
 *     - WiFi (inclusa nell’ESP32)
 *************************************************************/

/*************************************************************
 *  | GPIO | UTILIZZO    |
 *  |------|-------------|
 *  |  32  | MATRICE LED |
 *  |  22  |  SCL (Temp) |
 *  |  21  |  SDA (Temp) |
 *  |  04  | LUMINOSITA  |
 *************************************************************/

// ==== LIBRERIE ==== 
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

// ==== IMPOSTAZIONI ==== 
#include "wifi_settings.h"


// ==== DISPLAY ==== 
const unsigned int MATRIX_PIN = 32;

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(
  32, 8, MATRIX_PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + 
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800
);

// Posizione orologio
int positionClock = 2;

// Durata visualizzazione schermate
unsigned long screenDuration[] = {600, 10, 10};  // [secondi]
const int numScreens = sizeof(screenDuration) / sizeof(screenDuration[0]);
unsigned long lastSwitch = 0;
int currentScreen = 0;

// Luminosità
int brightnessDayHigh   = 30;    // Luminosità alta durante il giorno
int brightnessDayLow    = 20;    // Luminosità bassa durante il giorno
int brightnessNightHigh = 20;    // Luminosità alta durante la notte
int brightnessNightLow  = 3;     // Luminosità bassa durante la notte

int currentBrightness = brightnessDayLow;

// Colori (r,g,b)
uint32_t backgroundTimeDay     = matrix.Color( 14, 152, 227);  // Sfondo ora
uint32_t backgroundTimeNight   = matrix.Color(0,     0,   0);
uint32_t timeColorDay          = matrix.Color(255, 255, 255);  // Colore ora
uint32_t timeColorNight        = matrix.Color(255,   0,   0);

uint32_t backgroundDateDay     = matrix.Color( 14, 152, 227);  // Sfondo data
uint32_t backgroundDateNight   = matrix.Color(0,     0,   0);
uint32_t dateColorDay          = matrix.Color(255, 255, 255);  // Colore data
uint32_t dateColorNight        = matrix.Color(255,   0,   0);

uint32_t backgroundTempDay     = matrix.Color( 14, 152, 227);  // Sfondo temperatura
uint32_t backgroundTempNight   = matrix.Color(0,     0,   0);  // e umidità
uint32_t tempColorDay          = matrix.Color(255, 255, 255);  // Colore temperatura
uint32_t tempColorNight        = matrix.Color(255,   0,   0);  // e umidità

uint32_t dayColorDay           = matrix.Color(255, 255, 255);  // Colore giorno della
uint32_t dayColorNight         = matrix.Color(255,   0,   0);  // settimana presente
uint32_t notDayColorDay        = matrix.Color( 80,  80,  80);  // Colore altri giorni
uint32_t notDayColorNight      = matrix.Color(172,   0,   0);  // della settimana

bool dayColors = true; // true = usa colori notte


// ==== SENSORE LUMINOSITA ===
const unsigned int LUX_PIN = 34;
int luminosita = 0;

unsigned long tempLuxInterval = 1;
unsigned long lastLuxRead = 0;

unsigned int thresholdLuxDay = 1500;
unsigned int thresholdLuxNight = 1500;


// ==== SENSORE TEMPERATURA E UMIDITA ==== 
Adafruit_HTU21DF htu = Adafruit_HTU21DF();
float temp = 0;
float hum = 0;

float tempOffset = 0;   // Offset per tarare la lettura del
float humOffset = 0;    // sensore di temperatura e umidità

unsigned long tempReadInterval = 60;
unsigned long lastTempRead = 0;

// Buffer per formattare temperatura e umidita
String tempStr;


// ==== WEBSERVER E IMPOSTAZIONI ==== 
WebServer server(80);
Preferences preferences;


// ==== WIFI ====
// Timeout massimo per tentare la connessione WiFi (ms)
const unsigned long timeoutConnection = 10000;
const unsigned int MAXSSIDFOUND = 10;

const int sizeHomeSSID = sizeof(HomeSSID) / sizeof(HomeSSID[0]);

// Buffer reti trovate
String foundSSID[MAXSSIDFOUND];
String foundPASS[MAXSSIDFOUND];
int foundRSSI[MAXSSIDFOUND];
int foundCount = 0;
String connectedSSID;
bool connected = false;


// ==== DATA E ORA ==== 
// Server NTP
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* ntpServer3 = "time.google.com";

// Fuso orario italiano (ora legale inclusa)
const char* timeZoneItaly = "CET-1CEST,M3.5.0,M10.5.0/3";

// Sincronizzazione ogni 10 minuti (600 sec)
unsigned long syncInterval = 600;
unsigned long lastSync = 0;

// Offset per correzione secondi
int timeOffset = -2;

// Orari giorno/notte (HH:MM)
int dayStartHour     = 7;
int dayStartMinute   = 0;
int nightStartHour   = 22;
int nightStartMinute = 0;

struct tm timeinfo;
time_t now;

// Buffer per formattare orari e date
char timeStr[9];


// ==== SETUP ==== 
void setup() {
  delay(2000);

  // Serial debug
  Serial.begin(115200);
  while (!Serial) delay(1000);

  // Inizializzo LittleFS
  mountLittleFS();
  
  // Carico impostazioni salvate
  loadSettings();

  // Inizializzo la matrice
  Serial.println("Inizializzo display...");
  matrix.begin();
  background(0);
  matrix.setFont(&CapraFont);
  matrix.setTextWrap(false);
  matrix.setBrightness(brightnessDayHigh);
  matrix.setTextColor(timeColorDay);

  // Inizializzo wifi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);

  // Scansione reti e connessione
  scanWifi();
  sortByRSSI();
  tryConnection();

  // Sincronizzo orario
  syncTime();

  // Inizializzo sensore temperatura e umidita
  initTempHum();
  readTempHum();

  // Sensore luminosità
  analogReadResolution(12);
  updateBrightnessAndColors();

  // Avvio server web
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("Webserver avviato.");

  // Scrivo il logo 
  scrollText("CapraClock");

  // // Mostro prime schermate
  // showTime();
  // showDay();
}


// ==== LOOP ==== 
void loop() {
   // Sincronizza orario periodicamente
  if (millis() - lastSync > syncInterval * 1000) 
    syncTime();

  // Cambio schermata
  if (millis() - lastSwitch >= screenDuration[currentScreen] * 1000) {
    currentScreen = (currentScreen + 1) % numScreens;
    lastSwitch = millis();
  }

  if (millis() - lastTempRead >= tempReadInterval * 1000){
    readTempHum();
    lastTempRead = millis();
  }

  if (millis() - lastLuxRead >= tempLuxInterval * 1000){
    updateBrightnessAndColors();
    lastLuxRead = millis();
  }

  // Mostro schermata corrente
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

  // Gestione del web server
  server.handleClient();

  // delay(1);
}


// ==== FUNZIONI LUMINOSITA ==== 
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
    dayColors = true;  // di giorno usa sempre colori giorno
  } else {
    if (luminosita < thresholdLuxNight) {
      currentBrightness = brightnessNightLow;
      dayColors = false;   // poca luce = colori notte
    } else {
      currentBrightness = brightnessNightHigh;
      dayColors = true;  // tanta luce = colori giorno (anche se è notte)
    }
  }
}


// ==== FUNZIONI TEMPERATURA E UMIDITA ==== 
void initTempHum(){
  if (!htu.begin()) {
    Serial.println("HTU21D non trovato");
    while (1) scrollText("No sensore trovato");
  }
}

void readTempHum(){
  temp = htu.readTemperature();
  hum = htu.readHumidity();

  temp += tempOffset;
  hum += humOffset;

  Serial.print("Temperatura = ");
  Serial.print(temp);
  Serial.print("\t Umidita = ");
  Serial.println(hum);
}

void showTempHum() {
  tempStr = String(temp,1) + String("\xB0 ") + String(hum,0) + String("%");

  matrix.setBrightness(currentBrightness);
  background(dayColors ? backgroundTempDay : backgroundTempNight);
  matrix.setCursor(1, 6);
  matrix.setTextColor(dayColors ? tempColorDay : tempColorNight);
  matrix.print(tempStr);
}


// ==== FUNZIONI ORA/DATA ==== 
void syncTime() {
  Serial.print("Sincronizzo ora... ");
  configTzTime(timeZoneItaly, ntpServer1, ntpServer2, ntpServer3);

  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  Serial.println(timeStr);

  lastSync = millis();
}

bool isDaytime(struct tm *t) {
  int currentMinutes = t->tm_hour * 60 + t->tm_min;
  int dayStart   = dayStartHour * 60 + dayStartMinute;
  int nightStart = nightStartHour * 60 + nightStartMinute;

  if (dayStart < nightStart) {
    // Caso normale: giorno dentro la stessa giornata (es. 07:00–22:00)
    return (currentMinutes >= dayStart && currentMinutes < nightStart);
  } else {
    // Caso inverso: giorno attraversa la mezzanotte (es. 22:00–07:00)
    return (currentMinutes >= dayStart || currentMinutes < nightStart);
  }
}


// ==== FUNZIONI SCHERMATE ====
void showTime() {
  if (getLocalTime(&timeinfo)) {
    time(&now);
    now += timeOffset;
    localtime_r(&now, &timeinfo);

    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

    matrix.setBrightness(currentBrightness);
    background(dayColors ? backgroundTimeDay : backgroundTimeNight);
    matrix.setCursor(positionClock, 6);
    matrix.setTextColor(dayColors ? timeColorDay : timeColorNight);
    matrix.print(timeStr);
  } else {
    Serial.println("Errore: impossibile ottenere l'ora.");
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
    Serial.println("Errore: impossibile ottenere la data.");
    matrix.clear();
    matrix.show();
  }
}

void showDay() {
  if (getLocalTime(&timeinfo)) {
    int rectWidth = 3;
    int rectHeight = 1;
    int spacing = 1;
    int startX = positionClock;
    int y = 7; // ultima riga

    matrix.setBrightness(currentBrightness);

    for (int d = 0; d <= 6; d++) {
      int x = startX + d * (rectWidth + spacing);
      uint32_t color = (d == (timeinfo.tm_wday + 6) % 7) ? (dayColors ? dayColorDay : dayColorNight) : (dayColors ? notDayColorDay : notDayColorNight);
      
      matrix.fillRect(x, y, rectWidth, rectHeight, color);
    }
  } else {
    Serial.println("Errore: impossibile ottenere il giorno");
    matrix.clear();
    matrix.show();
  }
}


void background(uint32_t color){
  int rectWidth = 3;
  int rectHeight = 1;
  int spacing = 1;
  int startX = positionClock;
  int y = 7; // ultima riga

  matrix.setBrightness(currentBrightness);
  matrix.fillRect(0,0,matrix.width(),matrix.height()-1, color);

  matrix.fillRect(0, y, startX, rectHeight, color);
  matrix.fillRect(startX+rectWidth*1+spacing*0, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*2+spacing*1, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*3+spacing*2, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*4+spacing*3, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*5+spacing*4, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*6+spacing*5, y, spacing, rectHeight, color);
  matrix.fillRect(startX+rectWidth*7+spacing*6, y, matrix.width()-startX+rectWidth*7+spacing*6, rectHeight, color);
}


// ==== FUNZIONI IMPOSTAZIONI ==== 
void loadSettings() {   // Default settings
  preferences.begin("settings", true);

  // Posizione orologio
  positionClock       = preferences.getUInt("positionClock", positionClock);

  // Durata visualizzazione schermate
  screenDuration[0]   = preferences.getUInt("screenClock", screenDuration[0]);
  screenDuration[1]   = preferences.getUInt("screenDate", screenDuration[1]);
  screenDuration[2]   = preferences.getUInt("screenTemp", screenDuration[2]);

  // Luminosità
  brightnessDayHigh   = preferences.getUInt("brightnessDayHigh", brightnessDayHigh);
  brightnessDayLow    = preferences.getUInt("brightnessDayLow", brightnessDayLow);
  brightnessNightHigh = preferences.getUInt("brightnessNightHigh", brightnessNightHigh);
  brightnessNightLow  = preferences.getUInt("brightnessNightLow", brightnessNightLow);

  // Colori (r,g,b)
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

  // Luminosita
  thresholdLuxDay     = preferences.getUInt("thresholdLuxDay", thresholdLuxDay);
  thresholdLuxNight   = preferences.getUInt("thresholdLuxNight", thresholdLuxNight);

  // Temperatura e umidita
  tempOffset          = preferences.getUInt("tempOffset", tempOffset);
  humOffset           = preferences.getUInt("humOffset", humOffset);

  tempReadInterval    = preferences.getInt("tempReadInterval", tempReadInterval);

  // Data e ora
  syncInterval        = preferences.getUInt("syncInterval", syncInterval);
  timeOffset          = preferences.getInt("timeOffset", timeOffset);

  // Orari (formato HH:MM)
  dayStartHour        = preferences.getUInt("dayStartHour", dayStartHour);
  dayStartMinute      = preferences.getUInt("dayStartMinute", dayStartMinute);
  nightStartHour      = preferences.getUInt("nightStartHour", nightStartHour);
  nightStartMinute    = preferences.getUInt("nightStartMinute", nightStartMinute);

  preferences.end();
}

void saveSettings() {
  preferences.begin("settings", false);

  // Posizione orologio
  preferences.putUInt("positionClock", positionClock);
  
  // Durata visualizzazione schermate
  preferences.putUInt("screenClock", screenDuration[0]);
  preferences.putUInt("screenDate", screenDuration[1]);
  preferences.putUInt("screenTemp", screenDuration[2]);

  // Luminosità
  preferences.putUInt("brightnessDayHigh", brightnessDayHigh);
  preferences.putUInt("brightnessDayLow", brightnessDayLow);
  preferences.putUInt("brightnessNightHigh", brightnessNightHigh);
  preferences.putUInt("brightnessNightLow", brightnessNightLow);

  // Colori (r,g,b)
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

  // Luminosita
  preferences.putUInt("thresholdLuxDay", thresholdLuxDay);
  preferences.putUInt("thresholdLuxNight", thresholdLuxNight);

  // Temperatura e umidita
  preferences.putUInt("tempOffset", tempOffset);
  preferences.putUInt("humOffset", humOffset);

  preferences.putUInt("tempReadInterval", tempReadInterval);

  // Data e ora
  preferences.putUInt("syncInterval", syncInterval);
  preferences.putInt("timeOffset", timeOffset);

  // Orari (formato HH:MM)
  preferences.putUInt("dayStartHour", dayStartHour);
  preferences.putUInt("dayStartMinute", dayStartMinute);
  preferences.putUInt("nightStartHour", nightStartHour);
  preferences.putUInt("nightStartMinute", nightStartMinute);

  preferences.end();
}


// ==== LITTLE FS ==== 
void mountLittleFS(){
  if (!LittleFS.begin()) {
    Serial.println("Errore LittleFS");
    return;
  }

  // Lista file presenti in LittleFS
  Serial.println("File presenti in LittleFS:");
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("Errore: root non valido");
  } else {
    File file = root.openNextFile();
    while (file) {
      Serial.printf(" - %s (%d byte)\n", file.name(), file.size());
      file = root.openNextFile();
    }
  }
}


// ==== FUNZIONI WEB SERVER ==== 
void handleRoot() {
  String html;
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Errore: file HTML non trovato");
    return;
  }

  html = file.readString();
  file.close();
  // Sostituisco i segnaposto con valori reali

  // Posizione orologio
  html.replace("%POSITION_CLOCK%", String(positionClock));

  // Durata visualizzazione schermate
  html.replace("%SCREEN_CLOCK%", String(screenDuration[0]));
  html.replace("%SCREEN_DATE%", String(screenDuration[1]));
  html.replace("%SCREEN_TEMP%", String(screenDuration[2]));
  
  // Luminosità
  html.replace("%BRIGHTNESS_DAY_HIGH%", String(brightnessDayHigh));
  html.replace("%BRIGHTNESS_DAY_LOW%", String(brightnessDayLow));
  html.replace("%BRIGHTNESS_NIGHT_HIGH%", String(brightnessNightHigh));
  html.replace("%BRIGHTNESS_NIGHT_LOW%", String(brightnessNightLow));

  // Colori (r,g,b)
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

  // Luminosita
  html.replace("%THRESHOLD_LUX_DAY%", String(thresholdLuxDay));
  html.replace("%THRESHOLD_LUX_NIGHT%", String(thresholdLuxNight));

  // Temperatura e umidita
  html.replace("%TEMP_OFFSET%", String(tempOffset));
  html.replace("%HUM_OFFSET%", String(humOffset));
  
  html.replace("%TEMP_INTERVAL%", String(tempReadInterval));

  // Data e ora
  html.replace("%SYNC_INTERVAL%", String(syncInterval));
  html.replace("%TIME_OFFSET%", String(timeOffset));

  // Orari (formato HH:MM)
  char buf[6];
  sprintf(buf, "%02d:%02d", dayStartHour, dayStartMinute);
  html.replace("%DAY_START%", String(buf));
  sprintf(buf, "%02d:%02d", nightStartHour, nightStartMinute);
  html.replace("%NIGHT_START%", String(buf));

  server.send(200, "text/html", html);
}

void handleSave() {
  // Lettura e salvataggio dei valori numerici

  // Posizione orologio
  positionClock       = server.arg("positionClock").toInt();

  // Durata visualizzazione schermate
  screenDuration[0]   = server.arg("screenClock").toInt();
  screenDuration[1]   = server.arg("screenDate").toInt();
  screenDuration[2]   = server.arg("screenTemp").toInt();

  // Luminosità
  brightnessDayHigh   = server.arg("brightnessDayHigh").toInt();
  brightnessDayLow    = server.arg("brightnessDayLow").toInt();
  brightnessNightHigh = server.arg("brightnessNightHigh").toInt();
  brightnessNightLow  = server.arg("brightnessNightLow").toInt();

  // Colori (r,g,b)
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

  // Luminosita
  thresholdLuxDay     = server.arg("thresholdLuxDay").toInt();
  thresholdLuxNight   = server.arg("thresholdLuxNight").toInt();

  // Temperatura e umidita
  tempOffset          = server.arg("tempOffset").toInt();
  humOffset           = server.arg("humOffset").toInt();

  tempReadInterval    = server.arg("tempReadInterval").toInt();

  // Data e ora
  syncInterval        = server.arg("syncInterval").toInt();
  timeOffset          = server.arg("timeOffset").toInt();

  // Orari (formato HH:MM)
  if (server.hasArg("dayStart")) {
    sscanf(server.arg("dayStart").c_str(), "%d:%d", &dayStartHour, &dayStartMinute);
  }
  if (server.hasArg("nightStart")) {
    sscanf(server.arg("nightStart").c_str(), "%d:%d", &nightStartHour, &nightStartMinute);
  }

  saveSettings();

  server.sendHeader("Location", "/");
  server.send(303);
}

uint32_t htmlColorToMatrix(String hexColor) {
  if (hexColor[0] == '#') hexColor = hexColor.substring(1);
  long value = strtol(hexColor.c_str(), NULL, 16);
  uint8_t r = (value >> 16) & 0xFF;
  uint8_t g = (value >> 8) & 0xFF;
  uint8_t b = value & 0xFF;
  return matrix.Color(r, g, b);
}

String toHexColor(uint16_t color565) {
  // Estraggo i componenti RGB da 565
  uint8_t r5 = (color565 >> 11) & 0x1F;
  uint8_t g6 = (color565 >> 5)  & 0x3F;
  uint8_t b5 = color565 & 0x1F;

  // Converto in 8 bit (0-255)
  uint8_t r8 = (r5 * 255) / 31;
  uint8_t g8 = (g6 * 255) / 63;
  uint8_t b8 = (b5 * 255) / 31;

  char hexCol[8];
  sprintf(hexCol, "#%02X%02X%02X", r8, g8, b8);

  return String(hexCol);
}


// ==== FUNZIONI WIFI ==== 
void scanWifi() {
  Serial.println("Scansione reti WiFi...");
  int n = WiFi.scanNetworks();
  Serial.printf("Trovate %d reti.\n", n);

  if (n == 0) {
    Serial.println("Nessuna rete WiFi trovata.");
    while (true) scrollText("No WiFi trovato");
  }

  // Salvo solo le reti di casa trovate
  for (int i = 0; i < n && foundCount < MAXSSIDFOUND; i++) {
    for (int k = 0; k < sizeHomeSSID; k++) {
      if (WiFi.SSID(i) == HomeSSID[k]) {
        Serial.printf("Trovata rete %s (RSSI=%ld)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        foundSSID[foundCount] = WiFi.SSID(i);
        foundRSSI[foundCount] = WiFi.RSSI(i);
        foundCount++;
        break;
      }
    }
  }

  if (foundCount == 0) {
    Serial.println("Nessuna rete WiFi di casa trovata.");
    while (true) scrollText("No WiFi Casa trovato");
  }

  WiFi.scanDelete();
}

void sortByRSSI() {
  Serial.println("Ordino reti per potenza...");
  // Bubble sort decrescente
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

void tryConnection() {
  for (int i = 0; i < foundCount; i++) {
    Serial.printf("Connessione a %s...\n", foundSSID[i].c_str());
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(foundSSID[i].c_str(), HomePASS);

    unsigned long start = millis();
    while (millis() - start < timeoutConnection) {
      if (WiFi.status() == WL_CONNECTED) {
        connectedSSID = foundSSID[i];
        Serial.printf("Connesso a %s - IP: %s\n", connectedSSID.c_str(), WiFi.localIP().toString().c_str());
        scrollText(connectedSSID + String(" - ") + WiFi.localIP().toString());
        return;
      }
    }
    Serial.printf("Connessione a %s fallita.\n", foundSSID[i].c_str());
  }

  if (!connected) {
    Serial.println("Connessione con tutte le reti fallita.");
    while (true) scrollText("Non connesso");
  }
}


// ==== FUNZIONE TESTO SCORREVOLE ==== 
void scrollText(String text) {
  int textSize = text.length();
  int x = matrix.width();

  Serial.printf("Scrivo a schermo: %s\n", text.c_str());

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
















































