#include <WiFi.h>
#include "HX712.h"

// Replace with your network credentials
const char* ssid     = "ESP32-Access-Point";
const char* password = "1234";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// HX712 circuit wiring
const int LOADCELL_DOUT_PIN = 35;
const int LOADCELL_SCK_PIN = 34;
HX712 scale;
long offset=0;

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();

// power on offset
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  offset = getWeight(10);
  Serial.print("weight offset: ");
  Serial.println(offset);
}

void loop(){
  float reading;
  WiFiClient client = server.available();   // Listen for incoming clients

  reading = (float)(getWeight(3) - offset)/113000;
  //reading = getWeight(5);
  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println("Refresh: 2");  // refresh the page automatically every 5 sec
            client.println();
            client.println("<!DOCTYPE HTML>");
            client.println("<html>");
            client.print("<br>&nbsp;&nbsp;<span style=\"font-size: 50px\";>Weight: ");
            client.print(reading);
            client.print("kg");
            client.println("<br />");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
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
