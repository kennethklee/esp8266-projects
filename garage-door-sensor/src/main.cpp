#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <BlynkSimpleEsp8266.h>

#define TRIGGER_PIN 0
#define DOOR_PIN 2

BlynkTimer timer;
WidgetLED led1(V0);
char blynk_token[34] = "YOUR-BLYNK-TOKEN";

bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

volatile bool pinChanged = false;
volatile int  pinValue   = 0;

void checkPin()
{
  // Invert state, since button is "Active LOW"
  int newPinValue = !digitalRead(2);

  // Mark pin value changed
  if (newPinValue != pinValue) {
    pinValue = newPinValue;
    pinChanged = true;
  }
}

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
        json.printTo(Serial);
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
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 34);
  wifiManager.addParameter(&custom_blynk_token);

  wifiManager.setTimeout(300);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  if(!wifiManager.autoConnect("LeeGarage")) {
    //Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  //Serial.println("WiFi connected!");

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

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
}

void setup() {
  Serial.begin(74880);

  connectToWIFI();

  Blynk.config(blynk_token);

  pinMode(TRIGGER_PIN, INPUT);
//  pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(2), checkPin, CHANGE);

  // Update current state on start
  pinValue = !digitalRead(2);
  pinChanged = true;
}

void loop() {
  // put your main code here, to run repeatedly:
  Blynk.run();

  if (pinChanged) {
    // Process the value
    if (pinValue) {
      Serial.println("Door Closed");
      led1.off();
    } else {
      Serial.println("Door Open");
      led1.on();
    }

    // Clear the mark, as we have processed the value
    pinChanged = false;
  }

  if (digitalRead(TRIGGER_PIN) == LOW) {
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    delay(3000);
    ESP.reset();
    delay(5000);
  }
}
