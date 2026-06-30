#include <Arduino.h>
#include "driver.h"     // Must be included BEFORE TFT_eSPI to intercept configuration
#include <SPI.h>
#include <TFT_eSPI.h>   // Included via Seeed_GFX's integrated fork

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

const int screenWidth = 800;
const int screenHeight = 480;

// =========================================================================
// CUSTOM DEVICE CONFIGURATION (WiFi/Credentials are defined in driver.h)
// =========================================================================
const int httpsPort = 443;
const String url1 = "https://api.binance.com/api/v3/ticker?symbol=";
const String url2 = "&windowSize=1d";
const String cryptoCode = "BTC";
const String stockSymbol = "STX";

// NTP Server configuration for morning updates
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800; // Default to UTC+8. Adjust as needed.
const int daylightOffset_sec = 0;

WiFiClient client;
HTTPClient http;

// Geolocation coordinates
String latitude = "1.3521";
String longitude = "103.8198";
String location = "Singapore";
String timezone = "Asia/Singapore";

// Weather and display states
String current_date = "N/A";
String last_weather_update = "N/A";
String temperature = "--.-";
String humidity = "--";
int is_day = 1;
int weather_code = 0;
String NextDayForecast = "LOADING FORECAST...";
String allForecastSummaries = "";

// Crypto state variables
float cryptoPrice = 0.0;
float cryptoChangePercent = 0.0;
float cryptoHigh = 0.0;
float cryptoLow = 0.0;
bool cryptoFetchSuccess = false;

// Stock state variables
float stockPrice = 0.0;
float stockChangePercent = 0.0;
float stockHigh = 0.0;
float stockLow = 0.0;
bool stockFetchSuccess = false;

// Quote of the Day states
String quoteText = "Work as if you were to live 100 years. Pray as if you were to die tomorrow.";
String quoteAuthor = "Benjamin Franklin";
bool quoteFetchSuccess = false;

// Temperature scale flag (1 = Celsius, 0 = Fahrenheit)
#define TEMP_CELSIUS 1

#if TEMP_CELSIUS
  String temperature_unit = "";
  const char degree_symbol[] = " C";
#else
  String temperature_unit = "&temperature_unit=fahrenheit";
  const char degree_symbol[] = " F";
#endif

EPaper epaper;

// Function declarations
void get_weather_data();
void get_crypto_data();
void get_stock_data();
void get_quote_of_the_day();
void drawWrappedText(String text, int x, int y, int maxChars, int lineHeight);
float getUSDspotprice();
void drawDashboard();

// =========================================================================
// MAIN SETUP
// =========================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Initializing 7.5\" ePaper via TFT_eSPI...");
    
    // Initialize ePaper hardware
    epaper.init();
    epaper.fillScreen(TFT_WHITE);
    epaper.setRotation(4); 

    // Render static splash screen
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.setTextSize(3);
    epaper.setCursor(100, 200);
    epaper.print("Connecting to WiFi: ");
    epaper.setCursor(100, 250);
    epaper.print(ssid);
    epaper.update();

    // Start WiFi Connection using variables from driver.h
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[SUCCESS] WiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    } else {
        Serial.println("\n[WARNING] WiFi Connection failed. Running updates offline...");
    }

    // Fetch initial datasets
    get_weather_data();
    get_crypto_data();
    get_stock_data();
    get_quote_of_the_day();

    // Render the initial completed dashboard
    epaper.fillScreen(TFT_WHITE);
    drawDashboard();

    Serial.println("Initial ePaper paint refresh cycle starting...");
    epaper.update();
}

// =========================================================================
// MAIN LOOP
// =========================================================================
void loop() {
    struct tm timeinfo;
    static int lastUpdateHour = -1;

    // Check if we can fetch the synchronized local time
    if (getLocalTime(&timeinfo)) {
        // Daily scheduled telemetry updates at exactly 6:30 AM (hour 6) and 6:30 PM (hour 18)
        bool isTargetTime = (timeinfo.tm_min == 30 && (timeinfo.tm_hour == 6 || timeinfo.tm_hour == 18));
        
        if (isTargetTime) {
            if (lastUpdateHour != timeinfo.tm_hour) {
                lastUpdateHour = timeinfo.tm_hour;
                Serial.printf("Executing scheduled %02d:30 ePaper telemetry update...\n", timeinfo.tm_hour);
                if (WiFi.status() == WL_CONNECTED) {
                    get_weather_data();
                    get_crypto_data();
                    get_stock_data();
                    get_quote_of_the_day();

                    // Render updated telemetry to internal screen buffer
                    epaper.fillScreen(TFT_WHITE);
                    drawDashboard();

                    // Trigger physical e-ink state update
                    Serial.println("Pushing buffer to 7.5\" physical panel...");
                    epaper.update();
                } else {
                    Serial.printf("WiFi connection lost at %02d:30. Reconnecting...\n", timeinfo.tm_hour);
                    WiFi.disconnect();
                    WiFi.begin(ssid, password);
                    lastUpdateHour = -1;
                }
            }
        } else {
            if (timeinfo.tm_min != 30) {
                lastUpdateHour = -1;
            }
        }
    } else {
        // Fallback sentinel: If SNTP has not synchronized yet, run on a standard 15-minute interval
        static unsigned long lastUpdate = 0;
        unsigned long currentMillis = millis();

        if (currentMillis - lastUpdate >= 900000 || lastUpdate == 0) {
            lastUpdate = currentMillis;

            Serial.println("NTP not synchronized. Executing scheduled 15m backup update...");
            if (WiFi.status() == WL_CONNECTED) {
                get_weather_data();
                get_crypto_data();
                get_stock_data();
                get_quote_of_the_day();

                epaper.fillScreen(TFT_WHITE);
                drawDashboard();
                epaper.update();
            } else {
                Serial.println("WiFi connection lost. Attempting to reconnect...");
                WiFi.disconnect();
                WiFi.begin(ssid, password);
            }
        }
    }

    delay(1000);
}

// =========================================================================
// TELEMETRY INTEGRATIONS (CRYPTO, STOCK & WEATHER)
// =========================================================================

void get_crypto_data() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = url1 + cryptoCode + "USDT" + url2;
        Serial.print("Fetching Crypto Ticker: ");
        Serial.println(url);

        http.begin(url);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                cryptoPrice = doc["lastPrice"].as<float>();
                cryptoChangePercent = doc["priceChangePercent"].as<float>();
                cryptoHigh = doc["highPrice"].as<float>();
                cryptoLow = doc["lowPrice"].as<float>();
                cryptoFetchSuccess = true;

                Serial.printf("[SUCCESS] %s: Price: $%.2f, Change: %.2f%%\n", 
                              cryptoCode.c_str(), cryptoPrice, cryptoChangePercent);
            } else {
                Serial.print("[ERROR] Crypto JSON deserialize failed: ");
                Serial.println(error.c_str());
                cryptoFetchSuccess = false;
            }
        } else {
            Serial.printf("[ERROR] Crypto GET failed, HTTP code: %d\n", httpCode);
            cryptoFetchSuccess = false;
        }
        http.end();
    } else {
        Serial.println("[ERROR] WiFi unavailable. Skipping Crypto fetch.");
        cryptoFetchSuccess = false;
    }
}

void get_stock_data() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=" + stockSymbol + "&apikey=" + alphaVantageKey;
        Serial.print("Fetching Stock Ticker: ");
        Serial.println(url);

        http.begin(url);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                JsonObject quote = doc["Global Quote"];
                if (quote.containsKey("05. price")) {
                    stockPrice = quote["05. price"].as<float>();
                    
                    String pctStr = quote["10. change percent"].as<String>();
                    pctStr.replace("%", "");
                    stockChangePercent = pctStr.toFloat();
                    
                    stockHigh = quote["03. high"].as<float>();
                    stockLow = quote["04. low"].as<float>();
                    stockFetchSuccess = true;

                    Serial.printf("[SUCCESS] %s Stock: Price: $%.2f, Change: %.2f%%\n", 
                                  stockSymbol.c_str(), stockPrice, stockChangePercent);
                } else {
                    Serial.println("[WARNING] Stock payload empty or rate-limited.");
                    stockFetchSuccess = false;
                }
            } else {
                Serial.print("[ERROR] Stock JSON deserialize failed: ");
                Serial.println(error.c_str());
                stockFetchSuccess = false;
            }
        } else {
            Serial.printf("[ERROR] Stock GET failed, HTTP code: %d\n", httpCode);
            stockFetchSuccess = false;
        }
        http.end();
    } else {
        Serial.println("[ERROR] WiFi unavailable. Skipping Stock fetch.");
        stockFetchSuccess = false;
    }
}

void get_quote_of_the_day() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "https://favqs.com/api/qotd";
        Serial.print("Fetching Quote of the Day: ");
        Serial.println(url);

        http.begin(url);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                JsonObject quote = doc["quote"];
                if (quote.containsKey("body")) {
                    quoteText = quote["body"].as<String>();
                    quoteAuthor = quote.containsKey("author") ? quote["author"].as<String>() : "Unknown";
                    quoteFetchSuccess = true;

                    Serial.printf("[SUCCESS] Quote fetched: \"%s\" - %s\n", 
                                  quoteText.c_str(), quoteAuthor.c_str());
                } else {
                    Serial.println("[WARNING] Quote payload empty. Using fallback.");
                    quoteFetchSuccess = false;
                }
            } else {
                Serial.print("[ERROR] Quote JSON deserialize failed: ");
                Serial.println(error.c_str());
                quoteFetchSuccess = false;
            }
        } else {
            Serial.printf("[ERROR] Quote GET failed, HTTP code: %d\n", httpCode);
            quoteFetchSuccess = false;
        }
        http.end();
    } else {
        Serial.println("[ERROR] WiFi unavailable. Skipping Quote fetch.");
        quoteFetchSuccess = false;
    }
}

void drawWrappedText(String text, int x, int y, int maxChars, int lineHeight) {
    int start = 0;
    int currentY = y;
    while (start < text.length()) {
        if (start + maxChars >= text.length()) {
            epaper.setCursor(x, currentY);
            epaper.print(text.substring(start));
            break;
        }
        int spaceIdx = text.lastIndexOf(' ', start + maxChars);
        if (spaceIdx <= start) {
            spaceIdx = start + maxChars;
        }
        epaper.setCursor(x, currentY);
        epaper.print(text.substring(start, spaceIdx));
        start = spaceIdx + 1;
        currentY += lineHeight;
    }
}

void get_weather_data() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api-open.data.gov.sg/v2/real-time/api/twenty-four-hr-forecast";
    Serial.print("Fetching SG official Weather API: ");
    Serial.println(url);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc; 
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          JsonObject record = doc["data"]["records"][0];
          JsonObject general = record["general"];

          NextDayForecast = general["forecast"]["text"].as<String>();
          temperature = general["temperature"]["high"].as<String>();
          humidity = general["relativeHumidity"]["high"].as<String>();

          allForecastSummaries = "";
          JsonArray forecasts = record["forecasts"].as<JsonArray>();
          if (!forecasts.isNull()) {
            for (JsonVariant forecast : forecasts) {
              const char* day = forecast["day"];
              const char* summary = forecast["forecast"]["summary"];
              if (day && summary) {
                allForecastSummaries += String(day) + ": " + String(summary) + "\n";
              }
            }
          }

          String updatedTimestamp = record["updatedTimestamp"].as<String>();
          int splitIndex = updatedTimestamp.indexOf('T');
          if (splitIndex != -1) {
            current_date = updatedTimestamp.substring(0, splitIndex);
            last_weather_update = updatedTimestamp.substring(splitIndex + 1, splitIndex + 6);
          }

          Serial.printf("[SUCCESS] SG Weather: High Temp: %s, Humid: %s%%, Forecast: %s\n", 
                        temperature.c_str(), humidity.c_str(), NextDayForecast.c_str());
        } else {
          Serial.print("[ERROR] SG Weather JSON deserialize failed: ");
          Serial.println(error.c_str());
        }
      } else {
        Serial.printf("[ERROR] SG Weather API HTTP code: %d\n", httpCode);
      }
    } else {
      Serial.printf("[ERROR] GET request failed: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("[ERROR] WiFi unavailable. Skipping Weather fetch.");
  }
}

// =========================================================================
// GRAPHICS & LAYOUT DRAW ENGINE
// =========================================================================
void drawDashboard() {
    int midX = screenWidth / 2;   // 400
    int midY = screenHeight / 2;  // 240

    epaper.setTextColor(TFT_BLACK, TFT_WHITE);

    // Grid lines
    epaper.drawFastVLine(midX, 0, screenHeight, TFT_BLACK); 
    epaper.drawFastHLine(0, midY, screenWidth, TFT_BLACK);  

    // Q1: Weather Forecast
    epaper.setTextSize(2);
    epaper.setCursor(20, 20);
    epaper.print("WEATHER FORECAST");
    
    epaper.setTextSize(4);
    epaper.setCursor(30, 50);
    epaper.print(location);
    
    epaper.setTextSize(2);
    epaper.setCursor(30, 95);
    epaper.print("Date: ");
    epaper.print(current_date != "N/A" ? current_date : "2026-06-27");

    epaper.setTextSize(2);
    epaper.setCursor(30, 115);
    epaper.print("Next 24hr: ");
    epaper.setTextSize(3);
    epaper.setCursor(30, 145);
    epaper.print(NextDayForecast);

    epaper.setTextSize(5);
    epaper.setCursor(30, 180);
    epaper.print(temperature);
    epaper.setTextSize(3);
    epaper.print(degree_symbol);

    epaper.setTextSize(3);
    epaper.setCursor(150, 180);
    epaper.print("Humid: " + humidity + "%RH");

    // Q2: Stock Telemetry
    epaper.setTextSize(2);
    epaper.setCursor(midX + 20, 20);
    epaper.print("STOCK TELEMETRY");
    
    epaper.setTextSize(4);
    epaper.setCursor(midX + 35, 68);
    epaper.print(stockSymbol);
    
    epaper.setTextSize(5);
    epaper.setCursor(midX + 35, 130);
    if (stockFetchSuccess) {
        epaper.print("$");
        epaper.print(String(stockPrice, 2));
    } else {
        epaper.print("Connecting...");
    }
    
    epaper.setTextSize(2);
    epaper.setCursor(midX + 35, 175);
    epaper.print("Change: ");
    if (stockFetchSuccess) {
        if (stockChangePercent >= 0) epaper.print("+");
        epaper.print(String(stockChangePercent, 2));
        epaper.print("%");
    } else {
        epaper.print("--");
    }

    epaper.setCursor(midX + 35, 205);
    if (stockFetchSuccess) {
        epaper.print("High:$"); epaper.print(String(stockHigh, 2));
        epaper.print(" Low:$"); epaper.print(String(stockLow, 2));
    } else {
        epaper.print("High:-- Low:--");
    }

    // Q3: Crypto Watch
    epaper.setTextSize(2);
    epaper.setCursor(20, midY + 20);
    epaper.print("CRYPTOCURRENCY WATCH");
    
    epaper.setTextSize(4);
    epaper.setCursor(40, midY + 65);
    epaper.print(cryptoCode + " / USDT");
    
    epaper.setTextSize(4);
    epaper.setCursor(40, midY + 125);
    if (cryptoFetchSuccess) {
        epaper.print("$");
        epaper.print(String(cryptoPrice, 2));
    } else {
        epaper.print("Connecting...");
    }
    
    epaper.setTextSize(2);
    epaper.setCursor(40, midY + 185);
    epaper.print("24h Change: ");
    if (cryptoFetchSuccess) {
        if (cryptoChangePercent >= 0) epaper.print("+");
        epaper.print(String(cryptoChangePercent, 2));
        epaper.print("%");
    } else {
        epaper.print("--");
    }

    // Q4: Quote of the Day
    epaper.setTextSize(2);
    epaper.setCursor(midX + 20, midY + 20);
    epaper.print("QUOTE OF THE DAY");
    
    epaper.setTextSize(2);
    drawWrappedText("\"" + quoteText + "\"", midX + 35, midY + 65, 28, 20);
    
    epaper.setTextSize(2);
    epaper.setCursor(midX + 50, midY + 185);
    epaper.print("- ");
    epaper.print(quoteAuthor);
}