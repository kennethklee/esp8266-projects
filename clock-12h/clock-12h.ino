// 12 h clock
//#include <string>
static const String DAY[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
                                                    
// needed for Wifi Manager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
//#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//#include <WiFiUdp.h>

//const char *WIFI_SSID = "lee+=2";
//const char *WIFI_PASS = "letmein!";

// needed for OLED
#include "SSD1306.h"
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = 0;
const int SDC_PIN = 2;

SSD1306Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);

// needed for time
#include <NTPClient.h>  //https://github.com/arduino-libraries/NTPClient
const int UTC_OFFSET = -4;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, UTC_OFFSET * 60 * 60);

// needed for humidity sensor
#include "DHT.h"
#define DHTTYPE DHT11
#define DHTPIN 1
DHT dht(DHTPIN, DHTTYPE);

// functional buttons
#define FACTORY_RESET_PIN 3

void drawProgress(int percentage, String label) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, label);
  display.drawProgressBar(2, 28, 124, 10, percentage);
  display.display();
}

void executeReset(String message) {
  WiFiManager wifiManager;
  drawProgress(100, message);
  wifiManager.resetSettings();
  delay(2000);
  drawProgress(50, "Resetting");
  delay(1000);
  drawProgress(0, "Goodbye");
  delay(500);
  ESP.reset();
  drawProgress(0, "Please press reset if you see this.");
  delay(5000);
}

void configModeCallback(WiFiManager *_) {
  drawProgress(50, "Started AP: NTPClock");
}

int sampleSize = 5;
int samplePosition = 0;
float sampleHumiditySum = 0;
float sampleTempuratureSum = 0;
float humiditySample[] = {0, 0, 0, 0, 0};
float tempuratureSample[] = {0, 0, 0, 0, 0};

void renderDisplay() {
    int hours = timeClient.getHours();
    String ampm;
    if (hours >= 12) {
      ampm = "pm";
      hours -= 12;
    } else {
      ampm = "am";
    }
    int minutes = timeClient.getMinutes();
    String minStr = minutes > 10 ? String(minutes) : ("0" + String(minutes));

    display.clear();
    display.setFont(ArialMT_Plain_24);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 10, DAY[timeClient.getDay()]);
    display.drawString(64, 38, String(hours == 0 ? 12 : hours) + ":" + minStr + " " + ampm);

    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, String(sampleTempuratureSum / sampleSize) + "*C");
    
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(127, 0, String(sampleHumiditySum / sampleSize) + "%");

    // Debug
    // display.setTextAlignment(TEXT_ALIGN_LEFT);
    // display.drawString(0, 53, String(tempuratureSample[0]) + " " + String(tempuratureSample[1]) + " " + String(tempuratureSample[2]) + " " + String(tempuratureSample[3]) + " " + String(tempuratureSample[4]));
    
    display.display();
}

void setup() {
//  Serial.begin(74880);
  pinMode(FACTORY_RESET_PIN, INPUT);

  display.init();
//  display.flipScreenVertically();
  display.setContrast(127);

  WiFiManager wifiManager;
  wifiManager.setTimeout(300);
  wifiManager.setAPCallback(configModeCallback);
  drawProgress(33, "Connecting to WIFI");
  if(!wifiManager.autoConnect("NTPClock")) {
    executeReset("Timed out connecting to WIFI");
  }
  delay(500);
  
  drawProgress(66, "Retrieving Network Time");
  timeClient.begin();

  dht.begin();
  delay(1000);
}

String lastTime;
bool drawDot = true;
bool drawScreen = true;

void loop() {
  // Display time
  timeClient.update();

  String time = timeClient.getFormattedTime();
  if (lastTime != time) {
    drawScreen = true;

    lastTime = time;
  }

  // Display humidity & tempurature
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t) && t > 1) {
    // rolling average
    sampleHumiditySum = sampleHumiditySum + h - humiditySample[samplePosition];
    humiditySample[samplePosition] = h;

    sampleTempuratureSum = sampleTempuratureSum + t - tempuratureSample[samplePosition];
    tempuratureSample[samplePosition] = t;

    samplePosition = (samplePosition + 1 == 5) ? 0 : samplePosition + 1;

    drawScreen = true;
  }

  if (drawScreen) {
    renderDisplay();
  }

  // Blinking dot
  if (drawDot) {
    display.setPixel(127, 63);

    display.display();
  }
  drawDot = !drawDot;
  delay(2000);

  if (digitalRead(FACTORY_RESET_PIN) == LOW) {
    executeReset("Factory reset triggered.");
  }
}

