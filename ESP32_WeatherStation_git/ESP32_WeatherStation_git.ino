#include "ThingSpeak.h"
#include <WiFi.h>

#include <SDS011.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTPIN 26
#define DHTTYPE    DHT11

char ssid[] = "XXX";   // your network SSID (name) 
char pass[] = "XXX";   // your network password
int keyIndex = 0;            // your network key Index number (needed only for WEP)
WiFiClient  client;
unsigned long myChannelNumber = 11111;
const char * myWriteAPIKey = "your-thingspeak-api";
int number = 0;

SDS011 my_sds;
HardwareSerial port(2);

DHT_Unified dht(DHTPIN,  );

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);  //Initialize serial

  WiFi.mode(WIFI_STA);   
  ThingSpeak.begin(client);  // Initialize ThingSpeak
  
  my_sds.begin(&port);
  delay(1000);
  dht.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  float p10, p25;
  
  // Get temperature event and print its value.
  sensors_event_t event;

    
  // Connect or reconnect to WiFi
  if(WiFi.status() != WL_CONNECTED){
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);     
    } 
    Serial.println("\nConnected.");
  }

// SDS011
  int err = my_sds.read(&p25, &p10);
  if (!err) {
    Serial.println("P2.5: " + String(p25));
    Serial.println("P10:  " + String(p10));

  }

  // set the fields with the values
  ThingSpeak.setField(1, p25);
  ThingSpeak.setField(2, p10);

  dht.temperature().getEvent(&event);
  if (!(isnan(event.temperature))) {
    ThingSpeak.setField(3, event.temperature);
  }
  dht.humidity().getEvent(&event);
  if (!(isnan(event.relative_humidity))) {
    ThingSpeak.setField(4, event.relative_humidity);
  }    
  else{
    Serial.println("DHT problem");
  }  
  ThingSpeak.setStatus("test");

  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
//  delay(1000); // Wait 20 seconds to update the channel again
  
  delay(60000); // Wait 20 seconds to update the channel again
  delay(60000); // Wait 20 seconds to update the channel again
  delay(60000); // Wait 20 seconds to update the channel again
  delay(60000); // Wait 20 seconds to update the channel again
  delay(60000); // Wait 20 seconds to update the channel again
  delay(60000); // Wait 20 seconds to update the channel again
  delay(60000); // Wait 20 seconds to update the channel again
  delay(60000); // Wait 20 seconds to update the channel again
  delay(60000); // Wait 20 seconds to update the channel again
  delay(60000); // Wait 20 seconds to update the channel again
  ESP.restart();
}
