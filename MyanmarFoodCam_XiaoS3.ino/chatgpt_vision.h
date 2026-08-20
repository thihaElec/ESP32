#pragma once

#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "mbedtls/base64.h"

extern const char* OPENAI_API_KEY;

inline String askChatGPTMyanmarFood(camera_fb_t *fb) {
  if (!fb || fb->len == 0) {
    return "Error: Empty camera frame.";
  }

  // 1. Calculate required Base64 buffer size and allocate in PSRAM/Heap
  size_t base64_max_len = 4 * ((fb->len + 2) / 3) + 4;
  char *base64_buf = (char *)ps_malloc(base64_max_len);
  if (!base64_buf) {
    base64_buf = (char *)malloc(base64_max_len);
  }
  if (!base64_buf) {
    return "Error: Memory allocation failed for Base64 encoding.";
  }

  size_t actual_base64_len = 0;
  int encode_res = mbedtls_base64_encode(
    (unsigned char *)base64_buf, 
    base64_max_len, 
    &actual_base64_len, 
    fb->buf, 
    fb->len
  );

  if (encode_res != 0) {
    free(base64_buf);
    return "Error: Base64 encoding failed.";
  }
  base64_buf[actual_base64_len] = '\0'; // Null-terminate

  // 2. Prepare JSON segments with properly escaped strings
  const char* prompt_text = 
    "Identify this Myanmar (Burmese) dish. Provide:\\n"
    "1. Burmese Name (Myanmar script & English phonetics, e.g., မုန့်ဟင်းခါး - Mohinga)\\n"
    "2. Description & Region (e.g., Shan, Rakhine, Yangon)\\n"
    "3. Key Ingredients\\n"
    "4. How it is served and eaten\\n"
    "If not Myanmar food or not food at all, state that clearly.";

  String jsonPrefix = String("{\"model\":\"gpt-4o-mini\",\"messages\":[{\"role\":\"user\",\"content\":[") +
    "{\"type\":\"text\",\"text\":\"" + prompt_text + "\"}," +
    "{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/jpeg;base64,";

  String jsonSuffix = "\",\"detail\":\"low\"}}]}],\"max_tokens\":400}";

  // Exact Content-Length calculation
  size_t totalPayloadLen = jsonPrefix.length() + actual_base64_len + jsonSuffix.length();

  // 3. Connect via TLS
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  const char* host = "api.openai.com";
  Serial.printf("[OpenAI] Connecting to %s...\n", host);
  if (!client.connect(host, 443)) {
    free(base64_buf);
    return "Error: Could not connect to OpenAI server.";
  }

  // 4. Send HTTP Headers
  client.println("POST /v1/chat/completions HTTP/1.1");
  client.println("Host: api.openai.com");
  client.print("Authorization: Bearer ");
  client.println(OPENAI_API_KEY);
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.printf("Content-Length: %u\r\n\r\n", (unsigned int)totalPayloadLen);

  // 5. Send Body Parts
  client.print(jsonPrefix);

  // Send Base64 in 4KB chunks directly from the contiguous buffer
  size_t sent = 0;
  const size_t CHUNK = 4096;
  while (sent < actual_base64_len) {
    size_t chunk_len = (actual_base64_len - sent > CHUNK) ? CHUNK : (actual_base64_len - sent);
    client.write((const uint8_t*)(base64_buf + sent), chunk_len);
    sent += chunk_len;
    yield();
  }

  client.print(jsonSuffix);

  // We are done with the large Base64 buffer, free it immediately
  free(base64_buf);

  Serial.println("[OpenAI] Request sent successfully! Waiting for response...");

  // 6. Wait for HTTP Response
  unsigned long startWait = millis();
  while (!client.available()) {
    if (millis() - startWait > 12000) {
      client.stop();
      return "Error: Timeout waiting for OpenAI response.";
    }
    delay(20);
  }

  // 7. Skip Response Headers
  int contentLength = -1;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
      contentLength = line.substring(15).toInt();
    }
    if (line.length() == 0) {
      break;
    }
  }

  // 8. Read Response Body
  String responseBody = "";
  responseBody.reserve(2048);
  unsigned long readStart = millis();

  while (client.connected() || client.available()) {
    while (client.available()) {
      char c = (char)client.read();
      responseBody += c;
      readStart = millis();
    }
    if (contentLength > 0 && responseBody.length() >= (size_t)contentLength) {
      break;
    }
    if (responseBody.length() > 100 && (millis() - readStart > 1200)) {
      break;
    }
    delay(5);
  }
  client.stop();

  Serial.printf("[OpenAI] Received %u bytes response.\n", responseBody.length());

  // 9. Parse Response JSON
  int contentIndex = responseBody.indexOf("\"content\": \"");
  if (contentIndex != -1) {
    int startIndex = contentIndex + 12;
    int endIndex = startIndex;
    while (endIndex < responseBody.length()) {
      if (responseBody[endIndex] == '\"' && responseBody[endIndex - 1] != '\\') {
        break;
      }
      endIndex++;
    }

    String content = responseBody.substring(startIndex, endIndex);
    content.replace("\\n", "\n");
    content.replace("\\\"", "\"");
    content.replace("\\\\", "\\");
    return content;
  }

  // Return formatted error if ChatGPT returned a message
  if (responseBody.indexOf("\"error\"") != -1) {
    return "OpenAI Error:\n" + responseBody;
  }

  return responseBody.length() > 0 ? responseBody : "Error: Empty response.";
}