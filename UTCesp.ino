#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ezTime.h>

WiFiManager wm;

void setup() {
  WiFi.mode(WIFI_STA);
  Serial.begin(9600);
  bool conect;
  conect = wm.autoConnect("ANSEN WIFI FAILOVER");
  if(conect) {
    waitForSync();
    Serial.println(UTC.dateTime("~!~%YmdHis~^~&~@"));     
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
