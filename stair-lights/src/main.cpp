#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ArduinoOTA.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

#include <Adafruit_NeoPixel.h>

#define TRIGGER_PIN 2
#define LIGHT_COUNT 30
#define LIGHT_PIN 3
#define LIGHT_INTENSITY 100  // 1 - 255

Adafruit_NeoPixel pixels(LIGHT_COUNT, LIGHT_PIN, NEO_GRB + NEO_KHZ800);

void setColor(uint32_t r, uint32_t g, uint32_t b) {
  for (int i = 0; i < LIGHT_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
}

void errorOut() {
  setColor(LIGHT_INTENSITY, 0, 0);

  delay(3000);
  // reset and try again
  ESP.reset();
  delay(5000);
}

//callback notifying us of the need to save config
bool shouldSaveConfig = false;
void saveConfigCallback () {
  // Serial.println("Should save config");
  shouldSaveConfig = true;
}

char blynk_token[34] = "";

void connectToWIFI() {
  WiFiManager wifiManager;

  if (SPIFFS.begin()) {
    //Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      //Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        //Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        // json.printTo(Serial);
        // Serial.print("\n");
        if (json.success()) {
          //Serial.println("\nparsed json");
          strcpy(blynk_token, json["blynk_token"]);

        } else {
          //Serial.println("failed to load json config");
        }
      }
    }
  } else {
    //Serial.println("failed to mount FS");
    SPIFFS.format();
    wifiManager.resetSettings();
    errorOut();
  }
  
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 34);
  wifiManager.addParameter(&custom_blynk_token);

  wifiManager.setTimeout(300);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  if(!wifiManager.autoConnect("LeeStairsLight")) {
    // Serial.println("failed to connect and hit timeout");
    errorOut();
  }

  //if you get here you have connected to the WiFi
  strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    //Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      //Serial.println("failed to open config file for writing");
    }

    // json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
}


bool isOn = false;
int dayColor[3] = {LIGHT_INTENSITY, LIGHT_INTENSITY, LIGHT_INTENSITY};
int nightColor[3] = {255, 0, 0};
bool isNight = false;
WidgetLED led1(V0);
WidgetRTC rtc;
WidgetTerminal term(V6);

void beginOTA() {
  ArduinoOTA.onStart([]() {
    // String type;
    // if (ArduinoOTA.getCommand() == U_FLASH) {
    //   type = "sketch";
    // } else { // U_FS
    //   type = "filesystem";
    // }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    // Serial.println("Start updating " + type);
    term.println("Disconnecting because OTA starting...");
    term.flush();
    Blynk.disconnect();
  });
  ArduinoOTA.onEnd([]() {
    // Serial.println("\nEnd");
    term.println("OTA Finished...");
  });
  // ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  //   // Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  // });
  // ArduinoOTA.onError([](ota_error_t error) {
  //   Serial.printf("Error[%u]: ", error);
  //   if (error == OTA_AUTH_ERROR) {
  //     // Serial.println("Auth Failed");
  //   } else if (error == OTA_BEGIN_ERROR) {
  //     // Serial.println("Begin Failed");
  //   } else if (error == OTA_CONNECT_ERROR) {
  //     // Serial.println("Connect Failed");
  //   } else if (error == OTA_RECEIVE_ERROR) {
  //     // Serial.println("Receive Failed");
  //   } else if (error == OTA_END_ERROR) {
  //     // Serial.println("End Failed");
  //   }
  // });
  ArduinoOTA.begin();
}

void updateColor() {
  if (!isOn) {
    pixels.clear();
    pixels.show();
    led1.off();
    // term.println("");
    term.printf("%02d:%02d:%02d - No motion\n", hour(), minute(), second());
    term.flush();
    return;
  }

  term.printf("%02d:%02d:%02d - Motion detected!\n", hour(), minute(), second());
  term.flush();
  led1.on();
  if (isNight) {
    // term.printf("Night light change (%d, %d, %d)\n", nightColor[0], nightColor[1], nightColor[2]);
    setColor(nightColor[0], nightColor[1], nightColor[2]);
  } else {
    // term.printf("Day light change (%d, %d, %d)\n", dayColor[0], dayColor[1], dayColor[2]);
    setColor(dayColor[0], dayColor[1], dayColor[2]);
  }
}

void ICACHE_RAM_ATTR checkMotion() {
  // Invert state, since button is "Active LOW"
  bool pinValue = digitalRead(TRIGGER_PIN);

  // Mark pin value changed
  if (pinValue != isOn) {
    isOn = pinValue;
    updateColor();
  }
}

void setup() {
  // Serial.begin(9600);
  // Serial.println("Booting Stairs Light...");
  pinMode(TRIGGER_PIN, INPUT);

  pixels.begin();
  pixels.clear();

  connectToWIFI();
  // Serial.println("Connected to WIFI!");
  beginOTA();

  Blynk.config(blynk_token, "blynk.kennethklee.com", 8080);
  while (Blynk.connect() == false) {
  }
  // Serial.println("Connected to Blynk (blynk.kennethklee.com)");

  // Initial values
  isOn = digitalRead(TRIGGER_PIN);
  int timeHour = hour();
  isNight = timeHour < 6 || timeHour > 18;

  attachInterrupt(digitalPinToInterrupt(TRIGGER_PIN), checkMotion, CHANGE);
}

void loop() {
  ArduinoOTA.handle();
  Blynk.run();

  // Determine day or night
  int timeHour = hour();
  bool isNightTime = timeHour < 6 || timeHour > 18;  // Between 6am and 6pm is daytime
  if (isNightTime != isNight) {
    // On change
    isNight = isNightTime;
    updateColor();
  }
}

BLYNK_CONNECTED() {
  term.clear();
  term.print("Booted ");
  term.print(WiFi.localIP());
  term.print(", ");
  term.println(ArduinoOTA.getHostname());
  
  rtc.begin();
  Blynk.syncAll();
}

// Day
BLYNK_WRITE(V1) {
  dayColor[0] = param[0].asInt();
  dayColor[1] = param[1].asInt();
  dayColor[2] = param[2].asInt();

  updateColor();
}

// Night
BLYNK_WRITE(V2) {
  nightColor[0] = param[0].asInt();
  nightColor[1] = param[1].asInt();
  nightColor[2] = param[2].asInt();

  updateColor();
}