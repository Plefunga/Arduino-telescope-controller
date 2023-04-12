/*
  Copyright (C) 2023 Nathan Carter

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  To read the full terms and conditions, see https://www.gnu.org/licenses/.
*/

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ezTime.h>
HTTPClient http;

String serverName = "https://polaralign.nathancarter.repl.co";
String serial = "";
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
  if(Serial.available()>0){
    char c = Serial.read();
    serial += String(c);
    if(serial.endsWith("$A")){
      serial = "";
      WiFiClientSecure client;
      client.setInsecure();
      http.begin(client, serverName.c_str());
      int httpResponseCode = http.GET();
      if (httpResponseCode>0) {
        String payload = http.getString();
        Serial.println("#"+payload+"~");
      }
      else {
        Serial.println("#ERR:"+String(httpResponseCode)+"~");
      }
      http.end();
    }
    if(serial.endsWith("$GT")){
      serial = "";
      Serial.println(UTC.dateTime("~!~%YmdHis~^~&~@"));
    }
  }
}
