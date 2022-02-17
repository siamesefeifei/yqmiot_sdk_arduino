#include <ESP8266WiFi.h>
#include <yqmiot.h>

#define YQMIOT_WIFI_SSID "HomeWifi"
#define YQMIOT_WIFI_PASSWORD "88888888"
#define YQMIOT_NID 60161
#define YQMIOT_TOKEN "7d249a5fd340576b5764662aa809f4a0" // XXX 后期暂停使用NID

void setup() {
  Serial.begin(115200);
  Serial.println("\nYQMIOT v0.0.0\n");
  Serial.flush();

  // 连接WIFI
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(YQMIOT_WIFI_SSID, YQMIOT_WIFI_PASSWORD);
  for(int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
      Serial.print(".");
      delay(1000);
  }
  Serial.println("\nWifi connected.");
  Serial.flush();

  YQMIOT.begin(YQMIOT_NID, YQMIOT_TOKEN);
}

void loop() {
  YQMIOT.loop();
}
