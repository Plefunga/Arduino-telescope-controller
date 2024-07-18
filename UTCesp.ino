/*
  Copyright (C) 2024 Nathan Carter

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

//#define USEHTTPS 1 // comment out to use HTTP

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <ezTime.h>

#ifdef USEHTTPS
#include <WiFiClientSecure.h>
#else
#include <WiFiClient.h>
#endif

#define USEMDNS

//#define SERVER "https://polaralign.nathancarter.repl.co" // example server, use your own one running in your time zone.
//#define SERVER "http://172.27.158.4:5001/"

// don't include a slash at the end
#define SERVER "http://anssen-pc.local"


#define URL SERVER
#ifdef USEMDNS
#define TARGET_HOSTNAME ((String(SERVER).substring(String(SERVER).indexOf("/") + 2)).c_str())
#include <ESP8266mDNS.h>
#include <mDNSResolver.h>
IPAddress host = IPAddress(192,168,0,151);
WiFiUDP udp;
mDNSResolver::Resolver resolver(udp);
#undef URL
#define URL ("http://" + host.toString())
#endif

HTTPClient http;

String serial = "";
WiFiManager wm;

void setup()
{
  WiFi.mode(WIFI_STA);
  Serial.begin(9600);
  bool conect;
  conect = wm.autoConnect("ANSSEN WIFI FAILOVER");
  if(conect)
  {
    waitForSync(); 
  }
  #ifdef USEMDNS
  while(WiFi.status() != WL_CONNECTED)
  {
    delay(100);
  }
  delay(1000);
  resolver.setLocalIP(WiFi.localIP());
  host = resolver.search(TARGET_HOSTNAME);
  int count = 0;
  int max_count = 1;
  while(host == INADDR_NONE && count < max_count)
  {
    resolver.loop();
    //Serial.println(host);
    host = resolver.search(TARGET_HOSTNAME);
    count++;
  }
  // its still not working, so just hard code for the minute
  if(host == INADDR_NONE)
  {
    host = IPAddress(192,168,0,151);
  }
  #endif
}

void loop()
{
  if(Serial.available()>0)
  {
    char c = Serial.read();
    serial += String(c);

    if(serial.endsWith("$A"))
    {
      serial = "";

      bool got_index = false;
      
      while(got_index == false)
      {
          if(Serial.available() > 0)
          {
              char cc = Serial.read();
              if(cc == '|')
              {
                  got_index = true;
              }
              else
              {
                  serial += String(cc);
              }
          }
      }
      String index = serial;
      serial = "";

      #ifdef USEHTTPS
      WiFiClientSecure client;
      client.setInsecure();
      #else
      WiFiClient client;
      #endif

      
      //Serial.println((String(URL) + "/?star=" + index).c_str());
      http.begin(client, (String(URL) + "/?star=" + index).c_str());
      int httpResponseCode = http.GET();

      if (httpResponseCode>0)
      {
        String payload = http.getString();
        Serial.println("#"+payload+"~");
      }

      else {
        Serial.println("#ERR:"+String(httpResponseCode)+"~");
      }

      http.end();
    }

    if(serial.endsWith("$GT"))
    {
      serial = "";
      Serial.println(UTC.dateTime("~!~%YmdHis~^~&~@"));
    }
  }
}
