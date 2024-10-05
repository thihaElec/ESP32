#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

const char* ssid = "XXXXXXXX";
const char* password = "------------";

const int httpsPort = 443;
const String url1 = "https://api.binance.com/api/v3/ticker?symbol=";
const String url2 = "&windowSize=1d";
const String cryptoCode = "BTC";

WiFiClient client;
HTTPClient http;

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// End of constructor list
float getUSDspotprice(){
// get USD spot price
  http.begin(url1+"USDPUSDT");
  int httpCode = http.GET();
  StaticJsonDocument<2000> doc;
  DeserializationError error = deserializeJson(doc, http.getString());

  if (error) {
    Serial.print(F("deserializeJson Failed"));
    Serial.println(error.f_str());
    delay(2500);
    return 0.0;
  }

  Serial.print("HTTP Status Code: ");
  Serial.println(httpCode);

  String usdpusdt = doc["lastPrice"].as<String>();

  http.end();
  return usdpusdt.toFloat();
}

void setup(void) {
  Serial.begin(115200);

  u8g2.begin();
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  u8g2.setFont(u8g2_font_t0_13b_mr);

  u8g2.drawStr(0,10,"Crypto tracker");	// write something to the internal memory
  u8g2.sendBuffer();					// transfer internal memory to the display
  delay(2000);
}
void loop(void) {
  String crypto[] = {"BTCUSDT", "ETHUSDT", "ADAUSDT","XRPUSDT","SOLUSDT","DOTUSDT"}; 
  int arraysize = sizeof(crypto)/sizeof(crypto[0]);

  for(int ii=0; ii<arraysize;ii++)
  {
    Serial.println(arraysize);
    Serial.print("Connecting to ");
    Serial.println(url1+crypto[ii]+url2);

    http.begin(url1+crypto[ii]+url2);
    Serial.println(url1+crypto[ii]+url2);
    int httpCode = http.GET();
    StaticJsonDocument<2000> doc;
    DeserializationError error = deserializeJson(doc, http.getString());

    if (error) { //error handling
      Serial.print(F("deserializeJson Failed"));
      Serial.println(error.f_str());
      Serial.print("HTTP Status Code: ");
      Serial.println(httpCode);
  // Connect to WiFi network
      delay(5000);
      WiFi.disconnect();

      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("!!!!!!!!!!!!!!!!! connection lost");
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print("++++++++++++++++++++");
        }
      }
      else
      {
        http.end();
        Serial.println("*************** still connected");
        delay(5000);
      }
    continue;
    }
    Serial.print("HTTP Status Code: ");
    Serial.println(httpCode);

    String BTCUSDPrice = doc["lastPrice"].as<String>();
    String priceChange = doc["priceChange"].as<String>();
    String priceChangePct = doc["priceChangePercent"].as<String>();
    http.end();

    Serial.print("Getting history...");
    StaticJsonDocument<2000> historyDoc;

    Serial.print("BTCUSD Price: ");
    Serial.println(BTCUSDPrice.toDouble());

    u8g2.clearBuffer();					// clear the internal memory
    u8g2.setCursor(0,10);
    u8g2.print(crypto[ii]);
    u8g2.setCursor(0,25);
    u8g2.print(BTCUSDPrice);
    u8g2.setCursor(0,40);
    u8g2.print(priceChange);
    u8g2.setCursor(0,55);
    u8g2.print(priceChangePct);
    u8g2.sendBuffer();					// transfer internal memory to the display
    delay(3000);
  }
}