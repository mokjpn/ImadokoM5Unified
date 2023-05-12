# ImadokoM5Unified (M5Stampラウンド液晶モジュール対応版)

[ownTracks](https://owntracks.org/)から[MQTT](https://mqtt.org/)を用いて得られる位置情報をもとに、[OpenStreetMap](https://www.openstreetmap.org/)等の地図を用いて画像を生成するPHPスクリプトである[staticmap.php](https://github.com/Piskvor/staticMapLiteExt) で生成された地図画像をダウンロードして、[M5Stack](https://m5stack.com/)シリーズの画面上に地図を表示し続けるアプリケーションです。[M5Unified](https://github.com/lovyan03/M5Unified)ライブラリを用いているので、様々なM5Stackシリーズで動作します。ビルドは[PlatformIO](https://platformio.org/)上で行っています。
このブランチのバージョンは、[M5Stampラウンド液晶モジュール](https://ssci.to/8099)にM5Stamp C3Uを装着した状態に対応したものです。外部液晶の定義をサンプルプログラムから引用して追加しています。

## 必要なもの

- [ownTracks](https://owntracks.org/)からの情報がPublishされているMQTTサーバ
- [staticmap.php](https://github.com/Piskvor/staticMapLiteExt)が動作しているWebサーバ
- Wi-Fi

## ビルド方法

config-example.h をもとに、config.h を作成し、platformio.ini の boardを適切に設定してビルドしてください。

# ImadokoM5Unified

This is an application that downloads a map image generated by a PHP script called staticmap.php that generates an image using a map such as OpenStreetMap based on the location information obtained from ownTracks using MQTT, and displays the map on the screen of the M5Stack series. It uses the M5Unified library, so it works on various M5Stack series. The build is done on PlatformIO.

## Requirements

- MQTT server where information from [ownTracks](https://owntracks.org/) is published
- Web server where [staticmap.php](https://github.com/Piskvor/staticMapLiteExt) is running
- Wi-Fi

## How to build

Create config.h based on config-example.h and set the board in platformio.ini appropriately and build.
