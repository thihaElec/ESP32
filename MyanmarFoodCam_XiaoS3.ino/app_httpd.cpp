#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "chatgpt_vision.h"

httpd_handle_t camera_httpd = NULL;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>🇲🇲 Myanmar Food AI Camera</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .card { background: #1e293b; border-radius: 12px; padding: 20px; max-width: 680px; width: 100%; box-shadow: 0 4px 16px rgba(0,0,0,0.4); text-align: center; }
    h1 { color: #38bdf8; margin-top: 0; font-size: 22px; }
    .stream-container { position: relative; width: 100%; height: 360px; background: #000; border-radius: 8px; overflow: hidden; display: flex; align-items: center; justify-content: center; margin-bottom: 16px; border: 2px solid #334155; }
    .stream-container img { width: 100%; height: 100%; object-fit: contain; }
    .btn-group { display: flex; gap: 12px; justify-content: center; margin-bottom: 16px; }
    button { padding: 12px 24px; font-size: 16px; font-weight: 600; border: none; border-radius: 8px; cursor: pointer; transition: all 0.2s; }
    .btn-ai { background: #059669; color: white; }
    .btn-ai:hover { background: #10b981; }
    .btn-save { background: #3b82f6; color: white; }
    .btn-save:hover { background: #60a5fa; }
    button:disabled { background: #64748b; cursor: not-allowed; }
    .result-box { background: #0f172a; border: 1px solid #334155; border-radius: 8px; padding: 16px; text-align: left; white-space: pre-wrap; font-size: 15px; line-height: 1.6; color: #e2e8f0; min-height: 80px; }
    .spinner { display: inline-block; width: 18px; height: 18px; border: 3px solid rgba(255,255,255,.3); border-radius: 50%; border-top-color: #fff; animation: spin 1s linear infinite; vertical-align: middle; margin-right: 8px; }
    @keyframes spin { to { transform: rotate(360deg); } }
  </style>
</head>
<body>
  <div class="card">
    <h1>🇲🇲 Myanmar Food AI Identifier</h1>
    <p style="color: #94a3b8; font-size: 14px; margin-top: -8px;">Seeed Studio XIAO ESP32S3 + ChatGPT Vision</p>

    <div class="stream-container">
      <img id="stream" src="/stream" alt="Live Camera Stream">
    </div>

    <div class="btn-group">
      <button id="identifyBtn" class="btn-ai" onclick="identifyFood()">📸 Identify Myanmar Food</button>
      <button class="btn-save" onclick="downloadPhoto()">💾 Save Photo</button>
    </div>

    <div id="result" class="result-box">Aim the camera at a dish and click "Identify Myanmar Food".</div>
  </div>

  <script>
    async function identifyFood() {
      const btn = document.getElementById('identifyBtn');
      const resultBox = document.getElementById('result');
      const streamImg = document.getElementById('stream');

      btn.disabled = true;
      btn.innerHTML = '<span class="spinner"></span> Analyzing with ChatGPT (~3s)...';
      resultBox.innerText = 'Capturing frame and querying ChatGPT Vision API...';

      // Temporarily stop stream to release camera buffer for capture
      streamImg.src = '';

      try {
        const response = await fetch('/identify');
        const text = await response.text();
        resultBox.innerText = text;
      } catch (err) {
        resultBox.innerText = 'Request failed: ' + err.message;
      } finally {
        // Resume stream
        streamImg.src = '/stream';
        btn.disabled = false;
        btn.innerHTML = '📸 Identify Myanmar Food';
      }
    }

    function downloadPhoto() {
      window.location.href = '/capture';
    }
  </script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "30");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
      break;
    }

    size_t hlen = snprintf(part_buf, 64, _STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);

    esp_camera_fb_return(fb);
    fb = NULL;

    if (res != ESP_OK) break;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  return res;
}

static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=myanmar_food.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

static esp_err_t identify_handler(httpd_req_t *req) {
  Serial.println("\n[Food AI] Taking snapshot for ChatGPT...");

  // Grab fresh frame
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    const char* err_msg = "Error: Camera sensor busy or failed to capture.";
    httpd_resp_send_500(req);
    return httpd_resp_send(req, err_msg, strlen(err_msg));
  }

  // Process via OpenAI
  String result = askChatGPTMyanmarFood(fb);
  esp_camera_fb_return(fb);

  Serial.println("\n--- [ChatGPT Result] ---");
  Serial.println(result);
  Serial.println("------------------------\n");

  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, result.c_str(), result.length());
}

void setupLedFlash(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.stack_size = 10240;        // Increased stack size from 4KB to 10KB for TLS stability
  config.max_uri_handlers = 8;
  config.task_priority = 5;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t identify_uri = {
    .uri       = "/identify",
    .method    = HTTP_GET,
    .handler   = identify_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &identify_uri);
    Serial.println("HTTP Server started with 10KB stack!");
  }
}