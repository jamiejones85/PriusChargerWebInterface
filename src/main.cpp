/* 
  FSWebServer - Example WebServer with SPIFFS backend for esp8266
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the ESP8266WebServer library for Arduino environment.
 
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)
  or you can upload the contents of a folder if you CD in that folder and run the following command:
  for file in `ls -A1`; do curl -F "file=@$PWD/$file" esp8266fs.local/edit; done
  
  access the sample web page at http://esp8266fs.local
  edit the page by going to http://esp8266fs.local/edit
*/
/*
 * This file is part of the esp8266 web interface
 *
 * Copyright (C) 2018 Johannes Huebner <dev@johanneshuebner.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include "WebServer.h"

#define DBG_OUTPUT_PORT Serial
#define RESET_PIN D2

const char* host = "charger";
StaticJsonDocument<200> root;
bool sendSerial = false;

WebSocketsServer webSocket = WebSocketsServer(81);
WebServer webServer;

void staCheck();
void broadcastMessage(String message);
Ticker sta_tick(staCheck, 1000, 0, MICROS);
int8_t connectionNumber = 0;

void staCheck(){
  sta_tick.stop();
  if(!(uint32_t)WiFi.localIP()){
    WiFi.mode(WIFI_AP); //disable station mode
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
    switch(type) {
        case WStype_ERROR:      
        case WStype_DISCONNECTED:      
            connectionNumber = webSocket.connectedClients();
            break;
        case WStype_CONNECTED: 
            connectionNumber = webSocket.connectedClients();
            break;
        
        case WStype_TEXT: {
            String _payload = String((char *) &payload[0]);
            DeserializationError error = deserializeJson(root, _payload);
            sendSerial = root["showLog"];
            root["showLog"] = sendSerial;
            
            String response;
            serializeJson(root, response);

            webSocket.broadcastTXT(response);

          }
          break;  

    
             
        case WStype_BIN:
            break;
  
    }
}

void setup(void){
  Serial.begin(57600);
  Serial.setTimeout(100);
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, HIGH);

  //WIFI INIT
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin();
  sta_tick.start();
  
  MDNS.begin(host);
  
  //SERVER INIT
  ArduinoOTA.begin();

  webServer.setup(RESET_PIN);

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
}

void broadcastMessage(String message) {
  String response;
  root["message"] = message;
  root["timestamp"] = millis();
  serializeJson(root, response);
  webSocket.broadcastTXT(response);
}
 
void loop(void){
  ArduinoOTA.handle();
  webSocket.loop();
  webServer.loop();

  if(connectionNumber > 0 && sendSerial && Serial.available()) {
      String message = Serial.readString();
      if (message.length() > 0) {
        broadcastMessage(message);
      }
   }
}
