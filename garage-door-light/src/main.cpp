#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <BlynkSimpleEsp8266.h>

#include <Adafruit_NeoPixel.h>

#define LIGHT_PIN 2
#define LIGHT_INTENSITY 100  // 1 - 255

Adafruit_NeoPixel pixels(1, LIGHT_PIN, NEO_GRB + NEO_KHZ800);

//callback notifying us of the need to save config
bool shouldSaveConfig = false;
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

char blynk_token[34] = "";

void connectToWIFI() {
  WiFiManager wifiManager;
  pixels.setPixelColor(0, pixels.Color(LIGHT_INTENSITY / 2, LIGHT_INTENSITY / 2, LIGHT_INTENSITY / 2)); // White
  pixels.show();

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
  
  // Not sure why light doesn't stay on but hope this helps
  pixels.setPixelColor(0, pixels.Color(LIGHT_INTENSITY, LIGHT_INTENSITY, LIGHT_INTENSITY)); // White
  pixels.show();
  if(!wifiManager.autoConnect("LeeGarageLight")) {
    pixels.setPixelColor(0, pixels.Color(LIGHT_INTENSITY, 0, 0)); // Red
    pixels.show();
    // Serial.println("failed to connect and hit timeout");
    delay(3000);
    // reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
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

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
}


void setup() {
  Serial.begin(9600);

  pixels.begin();

  Serial.println("Booting Garage Light...");

  connectToWIFI();
  Serial.println("Connected to WIFI!");

  Blynk.config(blynk_token, "blynk.kennethklee.com", 8080);
  while (Blynk.connect() == false) {
  }
  Serial.println("Connected to Blynk (blynk.kennethklee.com)");

  // Unknown door state
  pixels.setPixelColor(0, pixels.Color(LIGHT_INTENSITY, LIGHT_INTENSITY, 0)); // Yellow
  pixels.show();
}


void loop() {
  Blynk.run();
  delay(1000);
}


int pinValue = -1;
WidgetLED led1(V0);

BLYNK_CONNECTED() {
  Blynk.syncAll();
}

BLYNK_WRITE(V1) {
  int newPinValue = param.asInt();

  if (newPinValue != pinValue) {
    pinValue = newPinValue;
    if (pinValue) {
      Serial.println("Door Closed");
      led1.off();
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
      pixels.show();
      pixels.clear();
    } else {
      Serial.println("Door Open");
      led1.on();
      pixels.setPixelColor(0, pixels.Color(0, LIGHT_INTENSITY, 0)); // Green
      pixels.show();
    }
  }
}