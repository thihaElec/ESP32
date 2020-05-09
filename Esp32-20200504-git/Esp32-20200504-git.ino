/******************************************************************************
 * Copyright 2018 Google
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "esp32-mqtt.h"
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include <ArduinoJson.h>
 
#define DHTTYPE DHT11
#define DHT_PIN 27
 
DHT dht(DHT_PIN,DHTTYPE);
 
char buffer[100];
float count=0;
 
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Setup.....");
//  dht.begin();
  pinMode(LED_BUILTIN, OUTPUT);
 
  setupCloudIoT(); 
}
 
unsigned long lastMillis = 0;
 
void loop() {
  mqtt->loop();
  delay(200);  // <- fixes some issues with WiFi stability
 
  if (!mqttClient->connected()) {
    connect();
    Serial.print("\nlast Error:");
    Serial.print(mqttClient->lastError());
    Serial.print("   return code:");
    Serial.println(mqttClient->returnCode());
    delay(5000);
  }

  if (millis() - lastMillis > 60000) {
    Serial.println("Publishing value");
    lastMillis = millis();
    float temp = count; //10.0; //dht.readTemperature();
    if (count++ >80)
        count = 0;
    float hum = 40.0; //dht.readHumidity();
    StaticJsonDocument<100> doc;
    doc["temp"] = temp;
    doc["humidity"] = hum;
    serializeJson(doc, buffer);
    //publishTelemetry(mqttClient, "/sensors", getDefaultSensor());
    publishTelemetry( buffer);
  }
}
