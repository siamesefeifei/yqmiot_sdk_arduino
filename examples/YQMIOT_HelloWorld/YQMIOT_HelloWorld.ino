#include <ESP8266WiFi.h>
#include <yqmiot.h>

const char WIFI_SSID[] = "HomeWifi";
const char WIFI_PASSWORD[] = "88888888";

#define YQMIOT_WIFI_SSID "HomeWifi"
#define YQMIOT_WIFI_PASSWORD "88888888"
#define YQMIOT_NID 10000
#define YQMIOT_TOKEN "aaaaaaaaaaaaa" // XXX 后期暂停使用NID

void setup() {
  Serial.begin(115200);
  Serial.println("\n");
  Serial.println("YQMIOT v0.0.0");
  Serial.println("start...");
  Serial.flush();

  // 连接WIFI
  Serial.println("wifi connect...");
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(YQMIOT_WIFI_SSID, YQMIOT_WIFI_PASSWORD);
  for(int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
      Serial.print(".");
      delay(1000);
  }
  Serial.println();
  Serial.println("wifi connected.");

  YQMIOT.begin(YQMIOT_NID, YQMIOT_TOKEN);
}

void loop() {

}
