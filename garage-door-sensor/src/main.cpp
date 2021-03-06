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

bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

char blynk_token[34] = "";
char bridge_blynk_token[34] = "";

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
        Serial.print("\n");
        if (json.success()) {
          //Serial.println("\nparsed json");
          strcpy(blynk_token, json["blynk_token"]);
          strcpy(bridge_blynk_token, json["bridge_blynk_token"]);

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
  WiFiManagerParameter custom_bridge_blynk_token("bridge blynk", "bridge blynk token", bridge_blynk_token, 34);
  wifiManager.addParameter(&custom_bridge_blynk_token);

  wifiManager.setTimeout(300);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  if(!wifiManager.autoConnect("LeeGarage")) {
    // Serial.println("failed to connect and hit timeout");
    delay(3000);
    // reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  strcpy(blynk_token, custom_blynk_token.getValue());
  strcpy(bridge_blynk_token, custom_bridge_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    //Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["blynk_token"] = blynk_token;
    json["bridge_blynk_token"] = bridge_blynk_token;

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


volatile bool isPinChanged = false;
volatile int  pinValue = 0;

void ICACHE_RAM_ATTR checkPin() {
  // Invert state, since button is "Active LOW"
  int newPinValue = !digitalRead(2);

  // Mark pin value changed
  if (newPinValue != pinValue) {
    pinValue = newPinValue;
    isPinChanged = true;
  }
}


WidgetLED led1(V0);
WidgetBridge bridge1(V1); 
BlynkTimer timer;

void syncState() {
  bridge1.virtualWrite(V1, pinValue);
  Serial.println("Sync state to bridge.");
}

void setup() {
  Serial.begin(9600);

  Serial.println("Booting Garage Sensor...");

  connectToWIFI();
  Serial.println("Connected to WIFI!");

  Blynk.config(blynk_token, "blynk.kennethklee.com", 8080);
  while (Blynk.connect() == false) {}
  Serial.println("Connected to Blynk (blynk.kennethklee.com)");

  // Serial.println("Setting up pins...");
  pinMode(TRIGGER_PIN, INPUT);
  // pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(2), checkPin, CHANGE);

  // Serial.println("Done!");

  // Update current state on start
  pinValue = !digitalRead(2);
  isPinChanged = true;

  timer.setInterval(5000L, syncState);
}

void loop() {
  // put your main code here, to run repeatedly:
  Blynk.run();
  timer.run();

  if (isPinChanged) {
    // Process the value
    if (pinValue) {
      Serial.println("Door Closed");
      led1.off();
    } else {
      Serial.println("Door Open");
      led1.on();
    }
    bridge1.virtualWrite(V1, pinValue);

    // Clear the mark, as we have processed the value
    isPinChanged = false;
  }

  // if (digitalRead(TRIGGER_PIN) == LOW) {
  //   WiFiManager wifiManager;
  //   wifiManager.resetSettings();
  //   delay(3000);
  //   ESP.reset();
  //   delay(5000);
  // }

  delay(500);
}

BLYNK_CONNECTED() {
  bridge1.setAuthToken(bridge_blynk_token);
}