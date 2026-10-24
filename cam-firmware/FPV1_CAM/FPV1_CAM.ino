/* FPV-1 camera node (Arduino IDE version): ESP32-CAM AI Thinker, OV3660, AP mode.
 * Hotspot "FPV-CAM" (WPA2: fpv12345). MJPEG: http://192.168.4.1:81/stream
 * Board: "AI Thinker ESP32-CAM". Partition: "Huge APP (3MB No OTA/1MB SPIFFS)".
 * Port: COM14 (FTDI), Upload speed 115200. GPIO0-GND + power-cycle, then Upload.
 */
#include "esp_camera.h"
#include "esp_http_server.h"
#include "img_converters.h"
#include <WiFi.h>

#define AP_SSID "FPV-CAM"
#define AP_PASS "fpv12345"

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static httpd_handle_t stream_httpd = NULL;
static httpd_handle_t camera_httpd = NULL;

static const char INDEX_HTML[] =
  "<!doctype html><html><head><meta charset=utf-8><meta name=viewport "
  "content='width=device-width,initial-scale=1'><title>FPV-CAM</title>"
  "<style>body{margin:0;background:#0b0d0e;color:#e8ecec;font-family:system-ui}"
  "img{width:100%;max-height:92vh;object-fit:contain;background:#000}"
  "p{font:13px ui-monospace,monospace;padding:8px 14px;color:#9aa3a3}</style></head>"
  "<body><img src='/stream' alt='live'>"
  "<p>FPV-CAM node - open the ground-station page on your laptop for record + telemetry.</p></body></html>";

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t status_handler(httpd_req_t *req) {
  static char json_response[1024];
  char *p = json_response;
  *p++ = '{';
  sensor_t *s = esp_camera_sensor_get();
  if (!s || !s->id.PID) return httpd_resp_send_500(req);
  p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
  p += sprintf(p, "\"quality\":%u,", s->status.quality);
  p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
  p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
  p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
  p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
  p += sprintf(p, "\"vflip\":%u,", s->status.vflip);
  p += sprintf(p, "\"colorbar\":%u", s->status.colorbar);
  *p++ = '}';
  *p++ = 0;
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t cmd_handler(httpd_req_t *req) {
  char *buf = NULL;
  char variable[32] = {0};
  char value[32] = {0};
  size_t buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (!buf) return httpd_resp_send_500(req);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) != ESP_OK ||
          httpd_query_key_value(buf, "val", value, sizeof(value)) != ESP_OK) {
        free(buf);
        return httpd_resp_send_404(req);
      }
    } else {
      free(buf);
      return httpd_resp_send_404(req);
    }
    free(buf);
  } else {
    return httpd_resp_send_404(req);
  }
  int val = atoi(value);
  sensor_t *s = esp_camera_sensor_get();
  int res = 0;
  if (!strcmp(variable, "framesize")) {
    if (s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
  } else if (!strcmp(variable, "quality")) res = s->set_quality(s, val);
  else if (!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
  else if (!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
  else if (!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
  else if (!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
  else if (!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
  else if (!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
  else if (!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
  else if (!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
  else if (!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
  else if (!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
  else if (!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
  else if (!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
  else if (!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
  else if (!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
  else if (!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
  else if (!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
  else if (!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
  else if (!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
  else if (!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
  else if (!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
  else if (!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
  else if (!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);
  else res = -1;
  if (res) return httpd_resp_send_500(req);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return httpd_resp_send_500(req);
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char part_buf[64];
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }
    if (fb->format != PIXFORMAT_JPEG) {
      bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!jpeg_converted) { res = ESP_FAIL; break; }
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }
    size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, _jpg_buf_len);
    res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    if (fb) { esp_camera_fb_return(fb); fb = NULL; _jpg_buf = NULL; }
    else if (_jpg_buf) { free(_jpg_buf); _jpg_buf = NULL; }
    if (res != ESP_OK) break;
  }
  return res;
}

static void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL};
  httpd_uri_t status_uri = {.uri = "/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL};
  httpd_uri_t cmd_uri = {.uri = "/control", .method = HTTP_GET, .handler = cmd_handler, .user_ctx = NULL};
  httpd_uri_t capture_uri = {.uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL};
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &status_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
  }
  config.server_port = 81;
  config.ctrl_port += 1;
  httpd_uri_t stream_uri = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL};
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
  Serial.println("camera server started: :80 ui/status/control/capture, :81/stream");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nFPV-1 camera node booting");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("camera init failed - check OV3660 seating");
    return;
  }
  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  s->set_framesize(s, FRAMESIZE_SVGA);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP up: ");
  Serial.println(AP_SSID);
  Serial.print("stream: http://");
  Serial.print(WiFi.softAPIP());
  Serial.println(":81/stream");

  startCameraServer();
}

void loop() {
  delay(10000);
}
