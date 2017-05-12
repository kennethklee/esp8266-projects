#include "SSD1306.h"

#define SENSOR_PIN 3
#define SENSOR_THRESHOLD_WET 250
#define SENSOR_THRESHOLD_DRY 400

// SSD1306 Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = 0;
const int SDC_PIN = 2;

SSD1306Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);

void setup() {
  Serial.begin(74880);

  display.init();
  display.flipScreenVertically();
  display.setContrast(255);
  display.setFont(ArialMT_Plain_16);
  
  pinMode(SENSOR_PIN, INPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  int sensorValue = analogRead(SENSOR_PIN);

  Serial.printf("Sensor Value: %04u, Moisture: %ld%\n", sensorValue, map(sensorValue, 0, 1023, 100, 0));

  String tip = sensorValue < SENSOR_THRESHOLD_WET ? "Too much water!" : sensorValue > SENSOR_THRESHOLD_DRY ? "Please water!" : "Happy!";

  display.clear();
  display.drawString(0, 14, "Moisture: " + String(map(sensorValue, 0, 1023, 100, 0)) + "%");
  display.drawString(0, 32, tip);
  display.display();

  delay(1000);
}
