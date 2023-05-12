#include <rpcWiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Seeed_Arduino_FS.h>
#define LGFX_USE_V1
#define LGFX_AUTODETECT
#include <HTTPClient.h>
#include <LovyanGFX.hpp>

// include WiFi / MQTT / staticmap settings
#include "config.h"

// WiFi client
WiFiClient wifiClient, wifiClient2;

// PubSubClient object
PubSubClient client(wifiClient);

// LovyanGFX object
static LGFX lcd;

// Unique Client ID for MQTT client identifier
String clientID;

uint8_t *imageData;
int imageDataIndex = 0;

#define DEFAULT_ZOOM 15
int zoom = DEFAULT_ZOOM;
String lat, lon;
String screen_size;

// URLを分解して、ホスト名とポート番号を取得する
struct UrlInfo {
  String host;
  int port;
};

UrlInfo parseUrl(const char *url) {
  String s = String(url);
  int start = s.indexOf('/') + 2;
  int end = s.indexOf('/', start);
  int port = s.startsWith("https://") ? 443 : 80; // set default ports
  int colon = s.indexOf(':', start);
  
  if (colon >= 0 && colon < end) {
    // port is specified in the URL
    port = s.substring(colon + 1, end).toInt();
    end = colon;
  }  
  return {s.substring(start, end), port};
}

void downloadAndDisplayImage(const char* url) {
  UrlInfo lurl = parseUrl(url);
  String host = lurl.host;
  int port = lurl.port;
  Serial.println(("Image Server Host: "+host).c_str());
  Serial.println(("Image Server Port: "+String(port)).c_str());
  if (!wifiClient2.connect(host.c_str(), port)) {
    Serial.println("Connection failed");
    return;
  }
  wifiClient2.print(String("GET ") + url + " HTTP/1.0\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (wifiClient2.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("Timeout");
      wifiClient2.stop();
      return;
    }
  }

  // HTTPヘッダを読み飛ばす
  bool isBody = false;
  String line;
  while (wifiClient2.available()) {
    line = wifiClient2.readStringUntil('\n');
    Serial.println(("Header: "+line).c_str());
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
      Serial.println("Timeout");
      wifiClient2.stop();
      break;
    }
  }

  Serial.printf("Image downloaded. Size: %d\n", imageDataIndex);
  lcd.drawJpg(imageData, imageDataIndex, 0, 0);
}

void reboot() {
  NVIC_SystemReset();
}

void updateMap() {

  String url = String(staticmap_url) + "?center=" + lat + "," + lon + "&zoom=" + String(zoom) + "&markers="+ lat + "," + lon + "&size=" + screen_size + "&format=jpg";
  Serial.println(url.c_str());

  // Download the JPEG image from the URL and display it on the canvas
  downloadAndDisplayImage(url.c_str());
 
}
// Callback function when a message is received from MQTT server
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
 for (int i = 0; i < length; i++) {
   Serial.print((char)payload[i]);
 }
  Serial.println();

  // Parse the payload as JSON and get the values of lat and lon
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  lat = String(doc["lat"].as<String>());
  lon = String(doc["lon"].as<String>());
  Serial.println("JSON lat: " + lat +", lon: "+ lon);
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
      Serial.println("connected");
      // Once connected, subscribe to the topic
      client.subscribe(mqtt_topic);
    } else {
      Serial.println("failed, rc="+ client.state());
      if(count++ > 5) {
        reboot();
      }
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // ボタンのセットアップ
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println("Wio start");
  imageData = (uint8_t *)malloc(BUFFERSIZE);
  Serial.println("LCD start");
  byte mac[6];
  WiFi.macAddress(mac);
  // MACアドレスを文字列に整形
  String macStr;
  for (int i = 0; i < 6; ++i) {
    if (mac[i] < 16) {
      macStr += "0";
    }
    macStr += String(mac[i], HEX);
  }
  clientID = "WioTerminal_" + macStr;
  // LCDのセットアップ
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(20);
  screen_size = String(lcd.width()) + "x" + String(lcd.height());
  lcd.setTextWrap(true, true);
  lcd.fillScreen(TFT_BLUE);
  lcd.setTextColor(TFT_WHITE);
  lcd.setFont(&fonts::Font4);
  lcd.println(F("[ImadokoWio]"));
  lcd.println();

  // Connect to WiFi network
  lcd.println();
  lcd.println();
  lcd.print("Connecting to ");
  lcd.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
    if (count++ > 20) {
      lcd.println("Failed to connect to WiFi");
      reboot();
    }
  }

  lcd.println("");
  lcd.println("WiFi connected");
  
  // Set the MQTT server and callback function
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
   if (!client.connected()) {
    reconnect();
   }
   client.loop();

  if (digitalRead(WIO_KEY_A) == LOW) {
    Serial.println("A Key pressed");
    zoom = DEFAULT_ZOOM;
    updateMap();
  }
   else if (digitalRead(WIO_KEY_B) == LOW) {
    Serial.println("B Key pressed");
    zoom++;
    updateMap(); 
   }
   else if (digitalRead(WIO_KEY_C) == LOW) {
    Serial.println("C Key pressed");
    zoom--;
    updateMap(); 
   }
}
