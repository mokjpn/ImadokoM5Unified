#include <HTTPClient.h>
#include <M5Unified.hpp>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LCBUrl.h>

// include WiFi / MQTT / staticmap settings
#include "config.h"
#include "lgfx/v1/panel/Panel_GC9A01.hpp"

// パネル定義 M5Stampラウンド液晶モジュール https://ssci.to/8099 
// LGFX_M5Stamp_SPI_GC9A01クラス定義は, サンプルプログラム https://github.com/urukakanko/M5Stamp_round_lcd/blob/main/M5stamp_ClockSample.ino より引用
class LGFX_M5Stamp_SPI_GC9A01 : public lgfx::LGFX_Device
{

lgfx::Panel_GC9A01 _panel_instance;
lgfx::Bus_SPI       _bus_instance;   // SPIバスのインスタンス
lgfx::Light_PWM     _light_instance;
public:
  LGFX_M5Stamp_SPI_GC9A01(void)
  {
    { // バス制御の設定を行います。
      auto cfg = _bus_instance.config();    // バス設定用の構造体を取得します。

// SPIバスの設定
      cfg.spi_host = SPI2_HOST;     // 使用するSPIを選択  ESP32-S2,C3 : SPI2_HOST or SPI3_HOST / ESP32 : VSPI_HOST or HSPI_HOST
      // ※ ESP-IDFバージョンアップに伴い、VSPI_HOST , HSPI_HOSTの記述は非推奨になるため、エラーが出る場合は代わりにSPI2_HOST , SPI3_HOSTを使用してください。
      cfg.spi_mode = 0;             // SPI通信モードを設定 (0 ~ 3)
      cfg.freq_write = 40000000;    // 送信時のSPIクロック (最大80MHz, 80MHzを整数で割った値に丸められます)
      cfg.freq_read  = 16000000;    // 受信時のSPIクロック
      cfg.spi_3wire  = true;        // 受信をMOSIピンで行う場合はtrueを設定
      cfg.use_lock   = true;        // トランザクションロックを使用する場合はtrueを設定
      cfg.dma_channel = SPI_DMA_CH_AUTO; // 使用するDMAチャンネルを設定 (0=DMA不使用 / 1=1ch / 2=ch / SPI_DMA_CH_AUTO=自動設定)
      // ※ ESP-IDFバージョンアップに伴い、DMAチャンネルはSPI_DMA_CH_AUTO(自動設定)が推奨になりました。1ch,2chの指定は非推奨になります。
      cfg.pin_sclk = 4;            // SPIのSCLKピン番号を設定
      cfg.pin_mosi = 6;            // SPIのMOSIピン番号を設定
      cfg.pin_miso = -1;            // SPIのMISOピン番号を設定 (-1 = disable)
      cfg.pin_dc   = 1;            // SPIのD/Cピン番号を設定  (-1 = disable)
     // SDカードと共通のSPIバスを使う場合、MISOは省略せず必ず設定してください。
      _bus_instance.config(cfg);    // 設定値をバスに反映します。
      _panel_instance.setBus(&_bus_instance);      // バスをパネルにセットします。
    }

    { // 表示パネル制御の設定を行います。
      auto cfg = _panel_instance.config();    // 表示パネル設定用の構造体を取得します。
      cfg.pin_cs           =    7;  // CSが接続されているピン番号   (-1 = disable)
      cfg.pin_rst          =    0;  // RSTが接続されているピン番号  (-1 = disable)
      cfg.pin_busy         =    -1;  // BUSYが接続されているピン番号 (-1 = disable)
      // ※ 以下の設定値はパネル毎に一般的な初期値が設定されていますので、不明な項目はコメントアウトして試してみてください。
      cfg.memory_width     =   240;  // ドライバICがサポートしている最大の幅
      cfg.memory_height    =   240;  // ドライバICがサポートしている最大の高さ
      cfg.panel_width      =   240;  // 実際に表示可能な幅
      cfg.panel_height     =   240;  // 実際に表示可能な高さ
      cfg.offset_x         =     0;  // パネルのX方向オフセット量
      cfg.offset_y         =     0;  // パネルのY方向オフセット量
      cfg.offset_rotation  =     0;  // 回転方向の値のオフセット 0~7 (4~7は上下反転)
      cfg.dummy_read_pixel =     8;  // ピクセル読出し前のダミーリードのビット数
      cfg.dummy_read_bits  =     1;  // ピクセル以外のデータ読出し前のダミーリードのビット数
      cfg.readable         =  true;  // データ読出しが可能な場合 trueに設定
      cfg.invert           =  true;  // パネルの明暗が反転してしまう場合 trueに設定
      cfg.rgb_order        = false;  // パネルの赤と青が入れ替わってしまう場合 trueに設定
      cfg.dlen_16bit       = false;  // データ長を16bit単位で送信するパネルの場合 trueに設定
      cfg.bus_shared       =  true;  // SDカードとバスを共有している場合 trueに設定(drawJpgFile等でバス制御を行います)

      _panel_instance.config(cfg);
    }
    { // バックライト制御の設定を行います。（必要なければ削除）
      auto cfg = _light_instance.config();    // バックライト設定用の構造体を取得します。

      cfg.pin_bl = 10;              // バックライトが接続されているピン番号
      cfg.invert = false;           // バックライトの輝度を反転させる場合 true
      cfg.freq   = 44100;           // バックライトのPWM周波数
      cfg.pwm_channel = 3;          // 使用するPWMのチャンネル番号

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);  // バックライトをパネルにセットします。
    }
    setPanel(&_panel_instance); // 使用するパネルをセットします。
  }
};

LGFX_M5Stamp_SPI_GC9A01 lcd;

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
  lcd.drawJpg(imageData, imageDataIndex, 0, 0);
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
  lcd.init();
  chipid = ESP.getEfuseMac(); // チップIDを取得
  // uint64_t型のchipidを16進数の文字列に変換  
  M5.Log.printf("ESP32 Chip ID = %016llX", chipid);
  M5_LOGI(); 
  uint32_t upper = chipid >> 32;
  uint32_t lower = (uint32_t) chipid;
  clientID = "M5Unified_" + String(upper, HEX) + String(lower, HEX);
//  lcd.setRotation(1);
//  lcd.setBrightness(20);
  screen_size = String(lcd.width()) + "x" + String(lcd.height());
  lcd.setTextWrap(true, true);
  lcd.fillScreen(TFT_BLUE);
  lcd.setTextColor(TFT_WHITE);
  lcd.setFont(&fonts::Font4);
  lcd.println(F("[ImadokoM5U]"));
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
