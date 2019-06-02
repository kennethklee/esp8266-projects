#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <DHT.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <BlynkSimpleEsp8266.h>

#define DHTTYPE    DHT11
#define DHTPIN 2

DHT dht(DHTPIN, DHTTYPE);

BlynkTimer timer;
char blynk_token[34] = "YOUR-BLYNK-TOKEN";

bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void sendSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // Celcius

  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  Blynk.virtualWrite(V0, t);
  Blynk.virtualWrite(V1, h);
}

void setup() {
  Serial.begin(9600);

  WiFiManager wifiManager;

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(blynk_token, json["blynk_token"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
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
  
  if(!wifiManager.autoConnect("RoomSensor")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected :)");

  strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  dht.begin();
  Blynk.config(blynk_token, "blynk.kennethklee.com");
  timer.setInterval(1000L, sendSensor);
}

void loop() {
  Blynk.run();
  timer.run();
}
