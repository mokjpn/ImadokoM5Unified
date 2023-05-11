#include <HTTPClient.h>
#include <M5Unified.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LCBUrl.h>

// include WiFi / MQTT / staticmap settings
#include "config.h"

// WiFi client
WiFiClient wifiClient, wifiClient2;

// PubSubClient object
PubSubClient client(wifiClient);

// Unique Chip ID for MQTT client identifier
uint64_t chipid;
String clientID;

uint8_t *imageData;
int imageDataIndex = 0;

#define DEFAULT_ZOOM 15
int zoom = DEFAULT_ZOOM;
String lat, lon;
String screen_size;

void downloadAndDisplayImage(const char* url) {
  LCBUrl lurl;
  lurl.setUrl(url);
  String host = lurl.getHost();
  int port = lurl.getPort();
  M5.Log(ESP_LOG_INFO, ("Image Server Host: "+host).c_str());
  M5.Log(ESP_LOG_INFO, ("Image Server Port: "+String(port)).c_str());
  if (!wifiClient2.connect(host.c_str(), port)) {
    M5_LOGI("Connection failed");
    return;
  }
  wifiClient2.print(String("GET ") + url + " HTTP/1.0\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (wifiClient2.available() == 0) {
    if (millis() - timeout > 5000) {
      M5_LOGI("Timeout");
      wifiClient2.stop();
      return;
    }
  }

  // HTTPヘッダを読み飛ばす
  bool isBody = false;
  String line;
  while (wifiClient2.available()) {
    line = wifiClient2.readStringUntil('\n');
    M5.Log(ESP_LOG_INFO, ("Header: "+line).c_str());
    if (line == "\r") {
      isBody = true;
    }
    if (isBody) {
      break;
    }
  }

  // 画像データをバッファに読み込む
  unsigned long lastDataTime = millis();
  imageDataIndex = 0;
  while (wifiClient2.available() && imageDataIndex < BUFFERSIZE) {
    uint8_t data = wifiClient2.read();
    imageData[imageDataIndex++] = data;
    lastDataTime = millis(); // データが読み込まれた最後の時刻を更新

    // タイムアウト判定
    if (millis() - lastDataTime > 5000) {
      M5_LOGI("Timeout");
      wifiClient2.stop();
      break;
    }
  }

  M5.Log.printf("Image downloaded. Size: %d\n", imageDataIndex);
  M5.Display.drawJpg(imageData, imageDataIndex, 0, 0);
}

void reboot() {
  ESP.restart();
}

void updateMap() {

  String url = String(staticmap_url) + "?center=" + lat + "," + lon + "&zoom=" + String(zoom) + "&markers="+ lat + "," + lon + "&size=" + screen_size + "&format=jpg";
  M5.Log(ESP_LOG_INFO, url.c_str());

  // Download the JPEG image from the URL and display it on the canvas
  downloadAndDisplayImage(url.c_str());
 
}
// Callback function when a message is received from MQTT server
void callback(char* topic, byte* payload, unsigned int length) {
  M5.Log.setSuffix(m5::log_target_serial, "");
  M5_LOGI("Message arrived [");
  M5.Log(ESP_LOG_INFO,topic);
  M5_LOGI("] ");
//  for (int i = 0; i < length; i++) {
//    M5.Log(ESP_LOG_INFO,(char)payload[i]);
//  }
  M5.Log.setSuffix(m5::log_target_serial, "\n");
  M5_LOGI();

  // Parse the payload as JSON and get the values of lat and lon
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  lat = String(doc["lat"].as<String>());
  lon = String(doc["lon"].as<String>());
  M5.Log(ESP_LOG_INFO,("JSON lat: " + lat +", lon: "+ lon).c_str());
  // Generate the image URL using the lat and lon values
  updateMap();
}

// Reconnect to MQTT server if disconnected
void reconnect() {
  int count = 0;
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientID.c_str(), mqtt_user, mqtt_password)) {
      M5_LOGI("connected");
      // Once connected, subscribe to the topic
      client.subscribe(mqtt_topic);
    } else {
      M5.Log(ESP_LOG_INFO, "failed, rc=", client.state());
      if(count++ > 5) {
        reboot();
      }
      M5_LOGI(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  auto cfg = M5.config(); // 設定用の構造体を代入。
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
  M5_LOGI("M5 start");
  imageData = (uint8_t *)malloc(BUFFERSIZE);
  M5_LOGI("LCD start");
  chipid = ESP.getEfuseMac(); // チップIDを取得
  // uint64_t型のchipidを16進数の文字列に変換  
  M5.Log.printf("ESP32 Chip ID = %016llX", chipid);
  M5_LOGI(); 
  uint32_t upper = chipid >> 32;
  uint32_t lower = (uint32_t) chipid;
  clientID = "M5Unified_" + String(upper, HEX) + String(lower, HEX);
//  M5.Display.setRotation(1);
//  M5.Display.setBrightness(20);
  screen_size = String(M5.Display.width()) + "x" + String(M5.Display.height());
  M5.Display.setTextWrap(true, true);
  M5.Display.fillScreen(TFT_BLUE);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.println(F("[ImadokoM5U]"));
  M5.Display.println();

  // Connect to WiFi network
  M5.Display.println();
  M5.Display.println();
  M5.Display.print("Connecting to ");
  M5.Display.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Display.print(".");
    if (count++ > 20) {
      M5.Display.println("Failed to connect to WiFi");
      reboot();
    }
  }

  M5.Display.println("");
  M5.Display.println("WiFi connected");
  
  // Set the MQTT server and callback function
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  M5.update();  
   if (!client.connected()) {
    reconnect();
   }
   client.loop();

  if (M5.BtnA.wasPressed()) {
    M5_LOGI("A Key pressed");
    zoom = DEFAULT_ZOOM;
    updateMap();
  }
   else if (M5.BtnB.wasPressed()) {
    M5_LOGI("B Key pressed");
    zoom++;
    updateMap(); 
   }
   else if (M5.BtnC.wasPressed()) {
    M5_LOGI("C Key pressed");
    zoom--;
    updateMap(); 
   }
}
