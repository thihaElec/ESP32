#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "HX712.h"

// HX712 circuit wiring
const int LOADCELL_DOUT_PIN = 35;
const int LOADCELL_SCK_PIN = 34;
HX712 scale;
long offset=0;

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;
const int readPin = 32; // Use GPIO number. See ESP32 board pinouts
const int LED = 2; // Could be different depending on the dev board. I used the DOIT ESP32 dev board.


//std::string rxValue; // Could also make this a global var to access it in loop()

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

//      if (rxValue.length() > 0) {
//        Serial.println("*********");
//        Serial.print("Received Value: ");
//
//        for (int i = 0; i < rxValue.length(); i++) {
//          Serial.print(rxValue[i]);
//        }
//
//        Serial.println();
//
//        // Do stuff based on the command received from the app
//        if (rxValue.find("A") != -1) { 
//          Serial.print("Turning ON!");
//          digitalWrite(LED, HIGH);
//        }
//        else if (rxValue.find("B") != -1) {
//          Serial.print("Turning OFF!");
//          digitalWrite(LED, LOW);
//        }
//        Serial.println();
//        Serial.println("*********");
//      }
    }
};
long getWeight(int count){
  long avg=0, current=0, previous=0;
  for (int ii=0;ii<count;ii++){
    while (scale.is_ready()==false);
    if (scale.is_ready()) {
      current = scale.read();
//      Serial.print("HX712 reading: ");
      Serial.println(current);
      if (ii>0)
      { 
        if ((current<(0.8*previous))||(current>(1.2*previous)))
        {//value is far too off
          ii--;
            Serial.println("Hererererere!!");
            avg =0;
            return avg;
        }
        else
        {
          avg+=current;
          previous=current;
        }
      }else{
        avg+=current;
        previous=current;
      }
    } else {
      Serial.println("HX712 not found.");
    }
    while (scale.is_ready()==true);    
  }
  avg /= count;
  Serial.println("Out!!");
  return avg;
}
void setup() {
  Serial.begin(115200);
  
  // Create the BLE Device
  BLEDevice::init("ESP32 UART Test"); // Give it a name

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
                      
  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

// power on offset
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  offset = getWeight(10);
}
void loop() {
  float reading;
  reading = (float)(getWeight(3) - offset)/113000;

  if (reading<0)
    reading = 0;
    
  if (deviceConnected) {
    // Fabricate some arbitrary junk for now...
    //txValue = analogRead(readPin) / 3.456; // This could be an actual sensor reading!
    txValue = reading;
    // Let's convert the value to a char array:
    char txString[8]; // make sure this is big enuffz
    dtostrf(txValue, 1, 2, txString); // float_val, min_width, digits_after_decimal, char_buffer
    pCharacteristic->setValue(txString);
    
    pCharacteristic->notify(); // Send the value to the app!
    Serial.print("*** Sent Value: ");
    Serial.print(reading);
    Serial.println(" ***");
  }
  delay(1000);
}
