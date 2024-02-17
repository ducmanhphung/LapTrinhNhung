#include "esp_camera.h"
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#define CAMERA_MODEL_AI_THINKER
#include <stdio.h>
#include "camera_pins.h"

#define FLASH_PIN 4
#define RELAY_1_PIN 12

const char* ssid = "Mang";       // Your wifi name like "myWifiNetwork"
const char* password = "12345678";  // Your password to the wifi network like "password123"
const char* websocket_server_host = "192.168.74.111";
const uint16_t websocket_server_port1 = 8888;

int flashlight = 0;
int relay1 = 0;

using namespace websockets;
WebsocketsClient client;
camera_config_t config;

void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    Serial.println("Connection Opened");
  } else if (event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("Connection Closed");
    ESP.restart();
  } else if (event == WebsocketsEvent::GotPing) {
    Serial.println("Got a Ping!");
  } else if (event == WebsocketsEvent::GotPong) {
    Serial.println("Got a Pong!");
  }
}

void onMessageCallback(WebsocketsMessage message) {
  String data = message.data();
  int index = data.indexOf("=");
  if (index != -1) {
    String key = data.substring(0, index);
    String value = data.substring(index + 1);

    if (key == "ON_BOARD_LED_1") {
      if (value.toInt() == 1) {
        flashlight = 1;
        digitalWrite(FLASH_PIN, HIGH);
      } else {
        flashlight = 0;
        digitalWrite(FLASH_PIN, LOW);
      }
    } else if (key == "RELAY_1") {
      if (value.toInt() == 1) {
        relay1 = 1;
        digitalWrite(RELAY_1_PIN, HIGH);
      } else {
        relay1 = 0;
        digitalWrite(RELAY_1_PIN, LOW);
      }
    } else if (key == "size") {
      int size = value.toInt();
      if (size == 0) {
        config.frame_size = FRAMESIZE_96X96;
      } else if (size == 1) {
        config.frame_size = FRAMESIZE_QCIF;
      } else if (size == 2) {
        config.frame_size = FRAMESIZE_240X240;
      } else if (size == 3) {
        config.frame_size = FRAMESIZE_VGA;
      } else if (size == 4) {
        config.frame_size = FRAMESIZE_HD;
      } else if (size == 5) {
        config.frame_size = FRAMESIZE_UXGA;
      }
    }

    Serial.print("Key: ");
    Serial.println(key);
    Serial.print("Value: ");
    Serial.println(value);
  }
}

void setup() {

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 40;
  config.fb_count = 2;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) { return; }

  sensor_t* s = esp_camera_sensor_get();

  s->set_contrast(s, 0);
  s->set_raw_gma(s, 1);
  // s->set_vflip(s, 1);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  pinMode(FLASH_PIN, OUTPUT);
  pinMode(RELAY_1_PIN, OUTPUT);

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);

  Serial.begin(115200);
  while (!client.connect(websocket_server_host, websocket_server_port1, "/")) { delay(500); }
}

void loop() {
  client.poll();

  // lấy dữ liệu hình ảnh từ camera
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    // không có hình ảnh thì giải phóng dữ liệu và đi đến vòng lặp tiếp theo
    esp_camera_fb_return(fb);
    return;
  }
  // Gửi hình ảnh đến websocket server
  client.sendBinary((const char*)fb->buf, fb->len);
  // giải phóng buffer
  esp_camera_fb_return(fb);
  String output = "ON_BOARD_LED_1=" + String(flashlight) + ",RELAY_1=" + String(relay1);
  Serial.println(output);
  // gửi các dữ liệu phụ đến server
  client.send(output);
}
