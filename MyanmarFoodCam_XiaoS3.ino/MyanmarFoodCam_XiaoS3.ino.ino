// capture image of the food and send to ChatGPT
// able to work, slow in image streaming, some error on ChatGPT probably due to image quality
//
#include "esp_camera.h"
#include <WiFi.h>
#include "camera_pins.h"
#include "chatgpt_vision.h"

// ==========================================
// 1. WiFi & OpenAI API Configurations
// ==========================================
const char* ssid     = "YourSSID";
const char* password = "YourPassword";

// Replace with your OpenAI API Key
const char* OPENAI_API_KEY = "your-api";

// Declared in app_httpd.cpp
void startCameraServer();
void setupLedFlash(int pin);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait up to 3s for USB Serial
  Serial.println("\n--- Starting XIAO ESP32S3 Myanmar Food Camera ---");

  // Camera Configuration for XIAO ESP32S3 Sense
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size   = FRAMESIZE_VGA;      // 640x480 (Ideal for Vision API)
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;                 // 10-14 gives clear details with compact payload
  config.fb_count     = 2;

  // Verify PSRAM is enabled (required on XIAO ESP32S3 Sense)
  if (!psramFound()) {
    Serial.println("WARNING: PSRAM not found! Reducing frame size to QVGA.");
    config.frame_size   = FRAMESIZE_QVGA;
    config.fb_location  = CAMERA_FB_IN_DRAM;
    config.fb_count     = 1;
  }

  // Camera Init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

// Warm up camera sensor to clear initial blank buffer
  for (int i = 0; i < 3; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
    }
    delay(50);
  }
  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, 1);       // Adjust sensor orientation if needed
    s->set_brightness(s, 1);  // Slight brightness boost for indoor food lighting
    s->set_saturation(s, 0);
  }

#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  // Connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Start the HTTP Camera & Identification Web Server
  startCameraServer();

  Serial.println("==================================================");
  Serial.print("Camera Server Ready! Open browser at: http://");
  Serial.println(WiFi.localIP());
  Serial.println("Click 'Identify Food / Save' to query ChatGPT.");
  Serial.println("==================================================");
}

void loop() {
  // FreeRTOS and esp_http_server tasks handle incoming requests in the background
  delay(10000);
}