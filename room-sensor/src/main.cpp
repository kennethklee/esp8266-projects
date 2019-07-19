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
#define DHTPIN 1

DHT dht(DHTPIN, DHTTYPE);
int sampleSize = 5;
int samplePosition = 0;
float sampleHumiditySum = 0;
float sampleTempuratureSum = 0;
float humiditySample[] = {0, 0, 0, 0, 0};
float tempuratureSample[] = {0, 0, 0, 0, 0};

BlynkTimer timer;
char blynk_token[34] = "YOUR-BLYNK-TOKEN";

bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback() {
  //Serial.println("Should save config");
  shouldSaveConfig = true;
}

void sendSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // Celcius

  if (!isnan(h) && !isnan(t) && t > 1) {
    // rolling average
    sampleHumiditySum = sampleHumiditySum + h - humiditySample[samplePosition];
    humiditySample[samplePosition] = h;

    sampleTempuratureSum = sampleTempuratureSum + t - tempuratureSample[samplePosition];
    tempuratureSample[samplePosition] = t;

    if (samplePosition + 1 == 5) {
      samplePosition = 0;
      Blynk.virtualWrite(V0, sampleTempuratureSum / sampleSize);
      Blynk.virtualWrite(V1, sampleHumiditySum / sampleSize);
    } else {
      samplePosition++;
    }
    //Serial.print(F("Humidity: "));
    //Serial.print(h);
    //Serial.println(F("% RH"));
    //Serial.print(F("Temperature: "));
    //Serial.print(t);
    //Serial.println(F("°C "));
  }


}

BLYNK_APP_CONNECTED() {
  //Serial.println("Blynk Connected.");
}

BLYNK_APP_DISCONNECTED() {
  //Serial.println("Blynk Disconnected.");
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
  
  if(!wifiManager.autoConnect("RoomSensor")) {
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
  //Serial.begin(9600);

  connectToWIFI()

  dht.begin();
  //Serial.println("Connecting to Blynk, blynk.kennethklee.com");
  Blynk.config(blynk_token, "blynk.kennethklee.com", 8080);
  // Blynk.config(blynk_token, IPAddress(10, 1, 1, 11), 8080);
  while (Blynk.connect() == false) {
  }
  //Serial.println("Connected to Blynk");
  timer.setInterval(1000L, sendSensor);
}

void loop() {
  Blynk.run();
  timer.run();
}
